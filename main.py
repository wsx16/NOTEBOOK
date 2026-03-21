import sys
import cv2
import numpy as np
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QLabel, QVBoxLayout, QHBoxLayout,
    QTableWidget, QTableWidgetItem, QPushButton, QFileDialog, QMessageBox,
    QInputDialog, QTabWidget, QTextEdit, QDialog, QListWidget, QListWidgetItem,
    QLineEdit, QComboBox, QHeaderView, QAbstractItemView,
)
from PyQt5.QtCore import Qt, QFileSystemWatcher, QThread, pyqtSignal
from PyQt5.QtGui import QImage, QPixmap
from ultralytics import YOLO
import hyperlpr3 as lpr3
from PIL import Image, ImageDraw, ImageFont
import Levenshtein
import pygame
import json
import logging
from logging.handlers import TimedRotatingFileHandler
from datetime import datetime
import os
import warnings
from db import (
    init_db,
    insert_plate_record, insert_plate_records,
    update_plate_record, fetch_plate_records,
    insert_rush_event, update_rush_plate, fetch_rush_events,
)

warnings.filterwarnings("ignore", category=DeprecationWarning)

# ── 常量 ──────────────────────────────────────────────────────────────────────
RUSH_EVENT_DIR    = "rush_events"
RUSH_EVENT_RECORD = os.path.join(RUSH_EVENT_DIR, "rush_records.json")
CONFIG_PATH       = "config.json"
LOG_PATH          = "system.log"
PLATE_RECORD_PATH = "plate_records.txt"

# ── 默认配置（保证所有 key 始终存在，避免 KeyError 崩溃）────────────────────
_DEFAULTS = {
    "model_path":           "best.pt",
    "font_path":            "simhei.ttf",
    "confidence_threshold": 0.975,
    "alert_sound":          "alert.wav",
}

# ── 字体缓存（避免每帧重新读磁盘）──────────────────────────────────────────
_font_cache: dict = {}


def _get_font(font_path: str, size: int = 30) -> ImageFont.FreeTypeFont:
    key = (font_path, size)
    if key not in _font_cache:
        _font_cache[key] = ImageFont.truetype(font_path, size)
    return _font_cache[key]


# ── 日志 ──────────────────────────────────────────────────────────────────────
def _setup_logging() -> logging.Logger:
    logger = logging.getLogger("SystemLogger")
    logger.setLevel(logging.DEBUG)
    if logger.handlers:
        return logger
    logger.propagate = False
    try:
        fh = TimedRotatingFileHandler(
            LOG_PATH, when="midnight", interval=1,
            backupCount=7, encoding="utf-8", delay=True,
        )
        fh.suffix = "%Y-%m-%d"
        fh.setLevel(logging.DEBUG)
        ch = logging.StreamHandler()
        ch.setLevel(logging.INFO)
        fmt = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
        fh.setFormatter(fmt)
        ch.setFormatter(fmt)
        logger.addHandler(fh)
        logger.addHandler(ch)
    except Exception as e:
        print(f"无法设置日志处理器: {e}")
    return logger


logger = _setup_logging()


# ── 配置加载 ──────────────────────────────────────────────────────────────────
def load_config() -> dict:
    config = dict(_DEFAULTS)
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            config.update(json.load(f))
        logger.info("配置文件加载成功")
    except FileNotFoundError:
        logger.warning("配置文件未找到，使用默认配置")
    except Exception as e:
        logger.error(f"加载配置文件失败: {e}，使用默认配置")
    return config


def save_config(config: dict) -> None:
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=4, ensure_ascii=False)


# ── 图像工具 ──────────────────────────────────────────────────────────────────
def draw_chinese_text_pil(
    image: np.ndarray,
    text: str,
    position: tuple,
    font_path: str,
    font_size: int = 30,
    color: tuple = (0, 255, 0),
) -> np.ndarray:
    image_pil = Image.fromarray(cv2.cvtColor(image, cv2.COLOR_BGR2RGB))
    draw = ImageDraw.Draw(image_pil)
    draw.text(position, text, font=_get_font(font_path, font_size), fill=color)
    return cv2.cvtColor(np.array(image_pil), cv2.COLOR_RGB2BGR)


# ── 视频推理线程 ───────────────────────────────────────────────────────────────
class VideoWorker(QThread):
    """在后台线程中完成逐帧读取、YOLO 推理、LPR 识别，通过信号回传结果。"""

    # (annotated_frame_bgr, rod_state_or_None)
    frame_ready    = pyqtSignal(object, object)
    plate_detected = pyqtSignal(str)
    video_finished = pyqtSignal()

    def __init__(
        self,
        cap: cv2.VideoCapture,
        yolo_model: YOLO,
        plate_catcher: lpr3.LicensePlateCatcher,
        config: dict,
    ):
        super().__init__()
        self.cap           = cap
        self.yolo_model    = yolo_model
        self.plate_catcher = plate_catcher
        self.config        = config
        self._paused       = False
        self._running      = True
        fps = cap.get(cv2.CAP_PROP_FPS)
        self._interval_ms  = max(1, int(1000 / fps)) if fps > 0 else 40

    def run(self) -> None:
        frame_count = 0
        while self._running:
            if self._paused:
                self.msleep(30)
                continue

            ret, frame = self.cap.read()
            if not ret:
                self.video_finished.emit()
                break

            rod_state = None

            # 每 10 帧检测车杆状态
            if frame_count % 10 == 0:
                results = self.yolo_model(frame, verbose=False)[0]
                boxes = results.boxes.xyxy.tolist()
                if boxes:
                    cls = int(results.boxes.cls.tolist()[0])
                    rod_state = "抬起" if cls == 0 else "关闭" if cls == 1 else "车杆损坏"
                else:
                    rod_state = "未检测到车杆"

            # 每 7 帧识别车牌
            if frame_count % 7 == 0:
                threshold = self.config.get("confidence_threshold", 0.975)
                font_path = self.config.get("font_path", "simhei.ttf")
                lpr_results = self.plate_catcher(frame)
                for plate_info in sorted(lpr_results, key=lambda x: x[1], reverse=True):
                    plate_no   = plate_info[0].upper().strip()
                    plate_conf = plate_info[1]
                    if plate_conf < threshold:
                        continue
                    x1, y1, x2, y2 = map(int, plate_info[3])
                    cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 0, 255), 2)
                    frame = draw_chinese_text_pil(
                        frame, plate_no,
                        (x1, max(0, y1 - 30)),
                        font_path, color=(0, 0, 255),
                    )
                    self.plate_detected.emit(plate_no)
                    break  # 只取置信度最高的一个

            self.frame_ready.emit(frame.copy(), rod_state)
            frame_count += 1
            self.msleep(self._interval_ms)

    def pause(self)  -> None: self._paused = True
    def resume(self) -> None: self._paused = False

    def stop(self) -> None:
        self._running = False
        self.cap.release()
        self.wait(3000)


# ── 数据统计页签 ───────────────────────────────────────────────────────────────
class DataStatsTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.rush_event_data: list = []
        self.file_watcher = QFileSystemWatcher()
        self.file_watcher.fileChanged.connect(self._on_file_changed)
        self._init_ui()
        self._load_all()
        # 只注册一次（修复原来三处重复注册的问题）
        self.file_watcher.addPaths([LOG_PATH, CONFIG_PATH, RUSH_EVENT_RECORD])

    def _init_ui(self) -> None:
        root = QVBoxLayout(self)
        self.tabs = QTabWidget()
        root.addWidget(self.tabs)

        # ── 车牌查询 ──────────────────────────────────────────────────────────
        plate_tab = QWidget()
        plate_layout = QVBoxLayout(plate_tab)
        search_row = QHBoxLayout()
        self.plate_search = QLineEdit()
        self.plate_search.setPlaceholderText("输入车牌号搜索…")
        self.plate_search.returnPressed.connect(self._search_plates)
        btn_plate_search  = QPushButton("搜索")
        btn_plate_refresh = QPushButton("刷新")
        btn_plate_search.clicked.connect(self._search_plates)
        btn_plate_refresh.clicked.connect(self._load_plate_table)
        search_row.addWidget(self.plate_search)
        search_row.addWidget(btn_plate_search)
        search_row.addWidget(btn_plate_refresh)
        self.plate_table_view = QTableWidget()
        self.plate_table_view.setColumnCount(3)
        self.plate_table_view.setHorizontalHeaderLabels(["来源视频", "过车时间", "车牌号"])
        self.plate_table_view.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.plate_table_view.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.plate_table_view.setSelectionBehavior(QAbstractItemView.SelectRows)
        plate_layout.addLayout(search_row)
        plate_layout.addWidget(self.plate_table_view)
        self.tabs.addTab(plate_tab, "车牌查询")

        # ── 系统日志（只读）───────────────────────────────────────────────────
        log_tab = QWidget()
        log_layout = QVBoxLayout(log_tab)
        self.log_edit = QTextEdit()
        self.log_edit.setReadOnly(True)          # 修复：日志不可手动编辑
        btn_log_refresh = QPushButton("刷新日志")
        btn_log_refresh.clicked.connect(self._load_log)
        log_layout.addWidget(self.log_edit)
        log_layout.addWidget(btn_log_refresh)
        self.tabs.addTab(log_tab, "系统日志")

        # ── 配置文件（可编辑）────────────────────────────────────────────────
        config_tab = QWidget()
        config_layout = QVBoxLayout(config_tab)
        self.config_edit = QTextEdit()
        btn_save_config = QPushButton("保存配置（重启后生效）")
        btn_save_config.clicked.connect(self._save_config)
        config_layout.addWidget(self.config_edit)
        config_layout.addWidget(btn_save_config)
        self.tabs.addTab(config_tab, "配置文件")

        # ── 冲岗情况（含搜索）────────────────────────────────────────────────
        rush_tab = QWidget()
        rush_layout = QVBoxLayout(rush_tab)
        rush_search_row = QHBoxLayout()
        self.rush_search = QLineEdit()
        self.rush_search.setPlaceholderText("输入车牌号搜索…")
        self.rush_search.returnPressed.connect(self._search_rush)
        self.rush_type_combo = QComboBox()
        self.rush_type_combo.addItems(["全部类型", "撞杆冲岗", "跟车冲岗"])
        btn_rush_search  = QPushButton("搜索")
        btn_rush_refresh = QPushButton("刷新")
        btn_rush_search.clicked.connect(self._search_rush)
        btn_rush_refresh.clicked.connect(self._load_rush_events)
        rush_search_row.addWidget(self.rush_search)
        rush_search_row.addWidget(self.rush_type_combo)
        rush_search_row.addWidget(btn_rush_search)
        rush_search_row.addWidget(btn_rush_refresh)
        self.rush_list = QListWidget()
        self.rush_list.itemDoubleClicked.connect(self._show_rush_detail)
        rush_layout.addLayout(rush_search_row)
        rush_layout.addWidget(self.rush_list)
        self.tabs.addTab(rush_tab, "冲岗情况")

    # ── 数据加载 ──────────────────────────────────────────────────────────────
    def _load_all(self) -> None:
        self._load_plate_table()
        self._load_log()
        self._load_config_file()
        self._load_rush_events()

    def _load_plate_table(self, plate_no: str = "") -> None:
        records = fetch_plate_records(limit=2000, plate_no=plate_no or None)
        self.plate_table_view.setRowCount(0)
        for r in records:
            row = self.plate_table_view.rowCount()
            self.plate_table_view.insertRow(row)
            self.plate_table_view.setItem(row, 0, QTableWidgetItem(r["video_name"]))
            self.plate_table_view.setItem(row, 1, QTableWidgetItem(r["timestamp"]))
            self.plate_table_view.setItem(row, 2, QTableWidgetItem(r["plate_no"]))

    def _search_plates(self) -> None:
        self._load_plate_table(self.plate_search.text().strip())

    def _load_log(self) -> None:
        if os.path.exists(LOG_PATH):
            try:
                with open(LOG_PATH, "r", encoding="utf-8") as f:
                    self.log_edit.setPlainText(f.read())
                self.log_edit.verticalScrollBar().setValue(
                    self.log_edit.verticalScrollBar().maximum()
                )
            except Exception as e:
                self.log_edit.setPlainText(f"读取日志失败: {e}")
        else:
            self.log_edit.setPlainText("日志文件不存在")

    def _load_config_file(self) -> None:
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                self.config_edit.setPlainText(
                    json.dumps(json.load(f), indent=4, ensure_ascii=False)
                )
        except Exception as e:
            self.config_edit.setPlainText(f"加载配置失败: {e}")

    def _save_config(self) -> None:
        try:
            config = json.loads(self.config_edit.toPlainText())
            with open(CONFIG_PATH, "w", encoding="utf-8") as f:
                json.dump(config, f, indent=4, ensure_ascii=False)
            QMessageBox.information(self, "提示", "配置已保存，重启程序后生效")
        except json.JSONDecodeError as e:
            QMessageBox.critical(self, "错误", f"JSON 格式错误: {e}")
        except Exception as e:
            QMessageBox.critical(self, "错误", f"保存失败: {e}")

    def _load_rush_events(self, plate_no: str = "", warning_type: str = "") -> None:
        self.rush_list.clear()
        events: list = []
        try:
            events = fetch_rush_events(
                limit=1000,
                plate_no=plate_no or None,
                warning_type=warning_type or None,
            )
        except Exception as e:
            logger.error(f"从数据库加载冲岗记录失败: {e}")

        # 数据库为空且无过滤时，兼容旧 JSON
        if not events and not plate_no and not warning_type and os.path.exists(RUSH_EVENT_RECORD):
            try:
                with open(RUSH_EVENT_RECORD, "r", encoding="utf-8") as f:
                    events = json.load(f)
            except Exception:
                pass

        self.rush_event_data = events
        for event in events:
            item   = QListWidgetItem()
            widget = QWidget()
            layout = QHBoxLayout(widget)
            layout.setContentsMargins(4, 4, 4, 4)

            img_label = QLabel()
            img_path  = event.get("image_path", "")
            if img_path and os.path.exists(img_path):
                img_label.setPixmap(QPixmap(img_path).scaled(100, 80, Qt.KeepAspectRatio))
            else:
                img_label.setText("无图片")
                img_label.setFixedSize(100, 80)

            info_label = QLabel(
                f"<b>时间：</b>{event.get('timestamp','')}<br>"
                f"<b>车牌：</b>{event.get('plate','')}<br>"
                f"<b>类型：</b>{event.get('warning_type','')}"
            )
            layout.addWidget(img_label)
            layout.addWidget(info_label)
            item.setSizeHint(widget.sizeHint())
            self.rush_list.addItem(item)
            self.rush_list.setItemWidget(item, widget)

    def _search_rush(self) -> None:
        plate_no = self.rush_search.text().strip()
        wt_text  = self.rush_type_combo.currentText()
        self._load_rush_events(
            plate_no=plate_no,
            warning_type="" if wt_text == "全部类型" else wt_text,
        )

    def _show_rush_detail(self, item: QListWidgetItem) -> None:
        index = self.rush_list.row(item)
        if not (0 <= index < len(self.rush_event_data)):
            return
        event = self.rush_event_data[index]
        dlg = QDialog(self)
        dlg.setWindowTitle("冲岗事件详情")
        layout = QVBoxLayout(dlg)

        img_label = QLabel()
        img_path  = event.get("image_path", "")
        if img_path and os.path.exists(img_path):
            img_label.setPixmap(QPixmap(img_path).scaled(800, 600, Qt.KeepAspectRatio))
        else:
            img_label.setText("截图文件不存在")

        info = QLabel(
            f"<b>发生时间：</b>{event.get('timestamp','')}<br>"
            f"<b>车牌号码：</b>{event.get('plate','')}<br>"
            f"<b>事件类型：</b>{event.get('warning_type','')}<br>"
            f"<b>详细信息：</b>{event.get('message','').replace(chr(10), '<br>')}"
        )
        info.setWordWrap(True)
        layout.addWidget(img_label)
        layout.addWidget(info)
        dlg.exec_()

    def _on_file_changed(self, path: str) -> None:
        if path == LOG_PATH:
            self._load_log()
        elif path == CONFIG_PATH:
            self._load_config_file()
        elif path == RUSH_EVENT_RECORD:
            self._load_rush_events()

    # ── 外部调用接口 ──────────────────────────────────────────────────────────
    def refresh_plates(self) -> None:
        self._load_plate_table(self.plate_search.text().strip())

    def refresh_rush(self) -> None:
        self._search_rush()


# ── 主窗口 ─────────────────────────────────────────────────────────────────────
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("基于机器视觉的高速收费站卡口冲岗行为识别系统")
        self.setMinimumSize(1280, 800)
        self.resize(1800, 1100)

        os.makedirs(RUSH_EVENT_DIR, exist_ok=True)
        init_db()

        self.config        = load_config()
        self.yolo_model    = YOLO(self.config["model_path"]).to("cuda")
        self.plate_catcher = lpr3.LicensePlateCatcher()
        pygame.mixer.init()

        # ── 检测状态变量 ──────────────────────────────────────────────────────
        self.current_video_path         = ""
        self.passed_plates: list        = []      # [(timestamp, plate_no), ...]
        self._passed_plate_set: set     = set()   # O(1) 查重
        self.vehicle_count              = 0
        self.previous_plate             = None
        self.current_plate              = ""
        self.current_up_plates: list    = []      # 本次开闸期间的车牌列表
        self.previous_rod_state         = None
        self.latest_frame               = None    # 最新帧（用于截图，避免额外读帧）
        self.rush_event_list: list      = []      # 内存冲岗记录（用于修改车牌联动）

        # 重命名 self.a / self.b → 语义清晰
        self.barrier_crash_recorded     = False   # 撞杆冲岗已记录
        self.tailgate_recorded          = False   # 跟车冲岗已记录

        self.worker: VideoWorker        = None

        self._build_ui()

    # ── UI 构建 ───────────────────────────────────────────────────────────────
    def _build_ui(self) -> None:
        self.setStyleSheet("""
            QPushButton {
                background: rgba(245,245,245,0.9);
                border: 1px solid #B0BEC5; border-radius: 5px;
                padding: 8px 15px; font: 14px '微软雅黑'; color: #2C3E50;
            }
            QPushButton:hover    { background: rgba(220,220,220,0.9); }
            QPushButton:pressed  { background: rgba(200,200,200,0.9); }
            QPushButton:disabled { background: rgba(220,220,220,0.6); color: #78909C; }
        """)

        root = QWidget()
        root_layout = QVBoxLayout(root)
        self.setCentralWidget(root)

        title = QLabel("高速收费站卡口冲岗行为识别系统")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet(
            "font: bold 30px '微软雅黑'; color: #2C3E50; padding: 12px;"
            "border-bottom: 3px solid #3498DB;"
        )
        root_layout.addWidget(title)

        self.tab_widget = QTabWidget()
        root_layout.addWidget(self.tab_widget)

        detect_tab = QWidget()
        self._build_detect_tab(detect_tab)
        self.tab_widget.addTab(detect_tab, "视频检测")

        self.stats_tab = DataStatsTab()
        self.tab_widget.addTab(self.stats_tab, "数据统计")

    def _build_detect_tab(self, tab: QWidget) -> None:
        layout = QHBoxLayout(tab)

        # 左：视频画面
        self.video_label = QLabel()
        self.video_label.setAlignment(Qt.AlignCenter)
        self.video_label.setStyleSheet("background: #000;")
        layout.addWidget(self.video_label, stretch=3)

        # 右：控制面板
        right = QWidget()
        right.setMaximumWidth(500)
        right_layout = QVBoxLayout(right)
        layout.addWidget(right, stretch=1)

        # 按钮区
        self.btn_open_file   = QPushButton("📁 选择视频文件")
        self.btn_open_camera = QPushButton("📷 打开摄像头")
        self.btn_pause       = QPushButton("⏸ 暂停")
        self.btn_pause.setEnabled(False)
        self.btn_open_file.clicked.connect(self.open_file_dialog)
        self.btn_open_camera.clicked.connect(self.open_camera)
        self.btn_pause.clicked.connect(self.toggle_pause)
        right_layout.addWidget(self.btn_open_file)
        right_layout.addWidget(self.btn_open_camera)
        right_layout.addWidget(self.btn_pause)

        # 状态标签
        label_style = (
            "font: bold 18px '微软雅黑'; color: #2C3E50; padding: 8px;"
            "background: rgba(255,255,255,0.7); border-bottom: 2px solid #3498DB;"
        )
        self.status_label = QLabel("车杆状态：未检测到车杆")
        self.count_label  = QLabel("通过车辆数目：0")
        self.status_label.setStyleSheet(label_style)
        self.count_label.setStyleSheet(label_style)
        right_layout.addWidget(self.status_label)
        right_layout.addWidget(self.count_label)

        # 报警标签（修复：setWordWrap 使 \n 正确换行）
        self.warning_label = QLabel()
        self.warning_label.setWordWrap(True)
        self.warning_label.setMinimumHeight(60)
        self.warning_label.setStyleSheet("color: red; font: bold 20px '微软雅黑';")
        right_layout.addWidget(self.warning_label)

        # 过车记录表格
        right_layout.addWidget(self._make_title("过车记录"))
        self.plate_table = QTableWidget()
        self.plate_table.setColumnCount(3)
        self.plate_table.setHorizontalHeaderLabels(["序号", "时间", "车牌号"])
        self.plate_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.plate_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.plate_table.setMaximumHeight(200)
        self.btn_modify = QPushButton("修改所选车牌")
        self.btn_modify.clicked.connect(self.modify_plate_number)
        right_layout.addWidget(self.plate_table)
        right_layout.addWidget(self.btn_modify)

        # 冲岗记录表格
        right_layout.addWidget(self._make_title("冲岗记录"))
        self.rush_table = QTableWidget()
        self.rush_table.setColumnCount(3)
        self.rush_table.setHorizontalHeaderLabels(["时间", "车牌号", "类型"])
        self.rush_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.rush_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.rush_table.setMaximumHeight(200)
        right_layout.addWidget(self.rush_table)
        right_layout.addStretch()

    @staticmethod
    def _make_title(text: str) -> QLabel:
        lbl = QLabel(text)
        lbl.setStyleSheet("font: bold 16px '微软雅黑'; margin-top: 6px;")
        return lbl

    # ── 视频 / 摄像头 ─────────────────────────────────────────────────────────
    def open_file_dialog(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "选择视频文件", "",
            "视频文件 (*.mp4 *.avi *.mov *.mkv);;所有文件 (*)",
        )
        # 修复：先确认选择了文件，再重置状态（避免取消对话框后清空已有数据）
        if not path:
            return
        self._reset_state()
        self._start_capture(cv2.VideoCapture(path), path)

    def open_camera(self) -> None:
        idx, ok = QInputDialog.getInt(self, "打开摄像头", "摄像头编号（通常为 0）：", 0, 0, 10)
        if not ok:
            return
        cap = cv2.VideoCapture(idx)
        if not cap.isOpened():
            QMessageBox.critical(self, "错误", f"无法打开摄像头 {idx}")
            return
        self._reset_state()
        self._start_capture(cap, f"camera_{idx}")

    def _start_capture(self, cap: cv2.VideoCapture, source_name: str) -> None:
        if not cap.isOpened():
            QMessageBox.critical(self, "错误", "无法打开视频源")
            return
        self.current_video_path = source_name
        logger.info(f"视频源: {os.path.basename(source_name)}")
        self.worker = VideoWorker(cap, self.yolo_model, self.plate_catcher, self.config)
        self.worker.frame_ready.connect(self._on_frame_ready)
        self.worker.plate_detected.connect(self._on_plate_detected)
        self.worker.video_finished.connect(self._on_video_finished)
        self.worker.start()
        self.btn_pause.setEnabled(True)
        self.btn_pause.setText("⏸ 暂停")

    def _reset_state(self) -> None:
        if self.worker is not None:
            self.worker.stop()
            self.worker = None

        self.passed_plates            = []
        self._passed_plate_set        = set()
        self.vehicle_count            = 0
        self.previous_plate           = None
        self.current_plate            = ""
        self.current_up_plates        = []
        self.previous_rod_state       = None
        self.latest_frame             = None
        self.rush_event_list          = []
        self.barrier_crash_recorded   = False
        self.tailgate_recorded        = False

        self.plate_table.setRowCount(0)
        self.rush_table.setRowCount(0)
        self.status_label.setText("车杆状态：未检测到车杆")
        self.count_label.setText("通过车辆数目：0")
        self.warning_label.clear()
        self.btn_pause.setEnabled(False)

    def toggle_pause(self) -> None:
        if self.worker is None:
            return
        if self.worker._paused:
            self.worker.resume()
            self.btn_pause.setText("⏸ 暂停")
        else:
            self.worker.pause()
            self.btn_pause.setText("▶️ 继续")

    # ── Worker 信号槽（主线程执行）────────────────────────────────────────────
    def _on_frame_ready(self, frame: np.ndarray, rod_state) -> None:
        # 保存最新帧（用于截图，不再从 cap 额外读帧）
        self.latest_frame = frame

        # 更新车杆状态
        if rod_state is not None:
            self.status_label.setText(f"车杆状态：{rod_state}")
            if rod_state != self.previous_rod_state:
                self._handle_rod_state_change(rod_state)
            self._handle_rod_state_no_change(rod_state)
            self.previous_rod_state = rod_state

        # 渲染到 UI（平滑缩放）
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb.shape
        q_img  = QImage(rgb.data, w, h, ch * w, QImage.Format_RGB888)
        pixmap = QPixmap.fromImage(q_img).scaled(
            self.video_label.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation
        )
        self.video_label.setPixmap(pixmap)

    def _on_plate_detected(self, plate_no: str) -> None:
        if not plate_no:
            return
        # O(1) 精确查重（已在本次视频中出现过的车牌直接跳过）
        if plate_no in self._passed_plate_set:
            return

        # 修复 Levenshtein 去抖顺序：先判断是否与上一辆相似，再决定是否追加
        if self.previous_plate and 0 < Levenshtein.distance(plate_no, self.previous_plate) < 3:
            logger.debug(f"去抖丢弃: {plate_no}（上一辆: {self.previous_plate}）")
            return

        # 确认为新车辆
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.passed_plates.append((timestamp, plate_no))
        self._passed_plate_set.add(plate_no)
        self.vehicle_count += 1
        self.previous_plate = plate_no
        self.current_plate  = plate_no
        self.count_label.setText(f"通过车辆数目：{self.vehicle_count}")

        # 实时写库（不再等到视频结束批量写入）
        try:
            insert_plate_record(self.current_video_path, timestamp, plate_no)
        except Exception as e:
            logger.error(f"车牌实时写库失败: {e}")

        # 更新过车表格
        row = self.plate_table.rowCount()
        self.plate_table.insertRow(row)
        self.plate_table.setItem(row, 0, QTableWidgetItem(str(self.vehicle_count)))
        self.plate_table.setItem(row, 1, QTableWidgetItem(timestamp))
        self.plate_table.setItem(row, 2, QTableWidgetItem(plate_no))
        logger.info(f"检测到新车牌: {plate_no}")

    def _on_video_finished(self) -> None:
        logger.info("视频播放完毕")
        self._save_plates_to_file()
        self.btn_pause.setEnabled(False)
        self.btn_pause.setText("⏸ 暂停")
        self.stats_tab.refresh_plates()
        QMessageBox.information(self, "提示", "视频播放完毕")

    # ── 冲岗检测逻辑 ──────────────────────────────────────────────────────────
    def _handle_rod_state_change(self, state: str) -> None:
        """仅在状态发生变化时调用。"""
        if state == "抬起":
            self.current_up_plates      = []
            self.tailgate_recorded      = False   # 新开闸周期重置
            self.barrier_crash_recorded = False
            self.warning_label.clear()
            logger.info("车杆抬起")
        elif state == "关闭":
            self.barrier_crash_recorded = False
            logger.info("车杆关闭")
        elif state == "未检测到车杆":
            self.barrier_crash_recorded = False

    def _handle_rod_state_no_change(self, state: str) -> None:
        """每次 YOLO 检测完毕都调用（含状态变化的那一帧）。"""
        if state == "车杆损坏":
            if self.current_plate and len(self.passed_plates) > 0:
                if not self.barrier_crash_recorded:
                    self.warning_label.setText(
                        f"警告：撞杆冲岗\n可疑车牌：{self.current_plate}"
                    )
                    self._play_alert()
                    self._record_rush_event(
                        datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                        self.current_plate,
                        "撞杆冲岗",
                    )
                    logger.warning(f"撞杆冲岗，可疑车牌: {self.current_plate}")
                    self.barrier_crash_recorded = True
        elif state in ("抬起", "关闭"):
            if not self._check_for_rush():
                self.warning_label.clear()

    def _check_for_rush(self) -> bool:
        """检测跟车冲岗：本次开闸内出现超过 1 辆车。"""
        if not self.current_plate:          # 修复：空车牌不追加
            return False
        if self.current_plate not in self.current_up_plates:
            self.current_up_plates.append(self.current_plate)
        if len(self.current_up_plates) > 1 and self.vehicle_count > 1:
            self.warning_label.setText(
                f"警告：跟车冲岗\n可疑车牌：{self.current_plate}"
            )
            self._play_alert()
            if not self.tailgate_recorded:
                self._record_rush_event(
                    datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                    self.current_plate,
                    "跟车冲岗",
                )
                logger.warning(f"跟车冲岗，可疑车牌: {self.current_plate}")
                self.tailgate_recorded = True
            return True
        return False

    def _record_rush_event(self, timestamp: str, plate_no: str, warning_type: str) -> None:
        """截图 + 写 JSON + 写数据库 + 更新 UI 冲岗表格。"""
        image_path = ""
        # 使用已保存的最新帧，不再从 cap 额外读一帧
        if self.latest_frame is not None:
            filename   = f"{timestamp.replace(':', '-')}_{plate_no}.jpg"
            image_path = os.path.join(RUSH_EVENT_DIR, filename)
            cv2.imwrite(image_path, self.latest_frame)

        message = f"警告：{warning_type}\n可疑车牌：{plate_no}"
        event   = {
            "timestamp":    timestamp,
            "plate":        plate_no,
            "message":      message,
            "warning_type": warning_type,
            "image_path":   image_path,
        }
        self.rush_event_list.append(event)

        # 写 JSON（兼容旧格式）
        events: list = []
        if os.path.exists(RUSH_EVENT_RECORD):
            try:
                with open(RUSH_EVENT_RECORD, "r", encoding="utf-8") as f:
                    events = json.load(f)
            except Exception:
                pass
        events.append(event)
        try:
            with open(RUSH_EVENT_RECORD, "w", encoding="utf-8") as f:
                json.dump(events, f, ensure_ascii=False, indent=2)
        except Exception as e:
            logger.error(f"写冲岗 JSON 失败: {e}")

        # 写数据库
        try:
            insert_rush_event(event, self.current_video_path)
        except Exception as e:
            logger.error(f"冲岗事件写库失败: {e}")

        # 更新冲岗表格
        row = self.rush_table.rowCount()
        self.rush_table.insertRow(row)
        self.rush_table.setItem(row, 0, QTableWidgetItem(timestamp))
        self.rush_table.setItem(row, 1, QTableWidgetItem(plate_no))
        self.rush_table.setItem(row, 2, QTableWidgetItem(warning_type))

        # 通知统计页刷新
        self.stats_tab.refresh_rush()

    # ── 修改车牌 ──────────────────────────────────────────────────────────────
    def modify_plate_number(self) -> None:
        selected = self.plate_table.selectedItems()
        if not selected or len(selected) != 3:
            QMessageBox.warning(self, "警告", "请先选中一行车牌")
            return
        row       = selected[0].row()
        old_plate = selected[2].text()
        new_plate, ok = QInputDialog.getText(
            self, "修改车牌号", "请输入新的车牌号：", text=old_plate
        )
        if not ok or not new_plate.strip():
            return
        new_plate = new_plate.strip().upper()

        # 更新 UI 表格
        self.plate_table.setItem(row, 2, QTableWidgetItem(new_plate))

        # 更新内存过车列表
        if 0 <= row < len(self.passed_plates):
            ts = self.passed_plates[row][0]
            self.passed_plates[row] = (ts, new_plate)
            self._passed_plate_set.discard(old_plate)
            self._passed_plate_set.add(new_plate)
            if self.current_plate  == old_plate: self.current_plate  = new_plate
            if self.previous_plate == old_plate: self.previous_plate = new_plate

            # 更新数据库（修复：原来只改内存，数据库始终是旧值）
            try:
                n1 = update_plate_record(old_plate, new_plate, ts, self.current_video_path)
                n2 = update_rush_plate(old_plate, new_plate)
                logger.info(f"车牌修改: {old_plate}→{new_plate}, plate_records {n1}行, rush_events {n2}行")
            except Exception as e:
                logger.error(f"数据库更新车牌失败: {e}")

            # 同步内存冲岗列表
            for ev in self.rush_event_list:
                if ev.get("plate") == old_plate:
                    ev["plate"] = new_plate

        # 追加修改记录到 txt
        try:
            with open(PLATE_RECORD_PATH, "a", encoding="utf-8") as f:
                f.write(f"[车牌修改] {old_plate} → {new_plate}\n")
        except Exception as e:
            logger.error(f"写入修改记录失败: {e}")

        QMessageBox.information(self, "提示", f"车牌已修改：{old_plate} → {new_plate}")

    # ── 辅助方法 ──────────────────────────────────────────────────────────────
    def _save_plates_to_file(self) -> None:
        """视频结束时将过车记录追加到 txt（仅作备份，主数据已实时写库）。"""
        if not self.passed_plates:
            return
        try:
            with open(PLATE_RECORD_PATH, "a", encoding="utf-8") as f:
                f.write(f"\n=== 视频文件: {os.path.basename(self.current_video_path)} ===\n")
                for ts, plate in self.passed_plates:
                    f.write(f"{ts} - {plate}\n")
            logger.info(f"车牌已保存: {PLATE_RECORD_PATH}\n" + "-" * 70)
        except Exception as e:
            logger.error(f"保存车牌文件失败: {e}")

    def _play_alert(self) -> None:
        try:
            if not pygame.mixer.music.get_busy():
                pygame.mixer.music.load(self.config["alert_sound"])
                pygame.mixer.music.play()
        except Exception as e:
            logger.error(f"播放报警音失败: {e}")

    def closeEvent(self, event) -> None:
        if self.worker is not None:
            self.worker.stop()
        pygame.mixer.music.stop()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
