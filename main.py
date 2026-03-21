import sys
import queue as _queue
import cv2
import numpy as np
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QLabel, QVBoxLayout, QHBoxLayout,
    QTableWidget, QTableWidgetItem, QPushButton, QFileDialog, QMessageBox,
    QInputDialog, QTabWidget, QTextEdit, QDialog, QListWidget, QListWidgetItem,
    QLineEdit, QComboBox, QHeaderView, QAbstractItemView,
)
from PyQt5.QtCore import Qt, QFileSystemWatcher, QThread, QTimer, pyqtSignal
from PyQt5.QtGui import QImage, QPixmap, QTextCursor
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
import time
import warnings
from db import (
    init_db,
    insert_plate_record, insert_plate_records,
    update_plate_record, fetch_plate_records,
    insert_rush_event, update_rush_plate, fetch_rush_events,
)

warnings.filterwarnings("ignore", category=DeprecationWarning)

# 同一车牌在同一会话内，5 分钟冷却后允许再次记录（防止长时间运行丢数据）
_PLATE_COOLDOWN_SEC  = 300
# 过车记录表格最多显示行数（超出时滚动丢弃最旧行，内存列表仍完整保留）
_PLATE_TABLE_MAX_ROWS = 200
# 推理帧队列容量（小队列保证推理始终消费最新帧，防止堆积卡顿）
_INFER_QUEUE_SIZE = 2

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


def validate_assets(config: dict) -> None:
    """启动时检查必需文件是否存在，缺失则弹框提示并退出。"""
    required = {
        "YOLO 模型":    config.get("model_path", "best.pt"),
        "字体文件":     config.get("font_path",  "simhei.ttf"),
        "报警音":       config.get("alert_sound", "alert.wav"),
    }
    missing = [f"{name}: {path}" for name, path in required.items() if not os.path.exists(path)]
    if missing:
        msg = "以下必需文件缺失，程序无法启动：\n\n" + "\n".join(missing)
        QMessageBox.critical(None, "文件缺失", msg)
        sys.exit(1)


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


# ── 读帧线程（以视频帧率读帧并推送至显示和推理两路消费者）──────────────────────
class FrameReaderThread(QThread):
    frame_ready    = pyqtSignal(object)   # raw BGR ndarray，供主线程显示
    video_finished = pyqtSignal()

    # 显示用的最大分辨率；缩小后主线程渲染负担大幅降低
    _DISPLAY_MAX_W = 1280
    _DISPLAY_MAX_H = 720

    def __init__(self, cap: cv2.VideoCapture, infer_queue: "_queue.Queue"):
        super().__init__()
        self.cap          = cap
        self.infer_queue  = infer_queue
        self._running     = True
        self._paused      = False
        fps = cap.get(cv2.CAP_PROP_FPS)
        self._interval_ms = max(1, int(1000 / fps)) if fps > 0 else 33
        # 预计算显示缩放比（只在源分辨率大于目标时缩小）
        src_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        src_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        scale = min(self._DISPLAY_MAX_W / max(src_w, 1),
                    self._DISPLAY_MAX_H / max(src_h, 1))
        if scale < 1.0:
            self._disp_w = int(src_w * scale)
            self._disp_h = int(src_h * scale)
        else:
            self._disp_w = src_w
            self._disp_h = src_h

    def run(self) -> None:
        try:
            while self._running:
                if self._paused:
                    self.msleep(30)
                    continue
                ret, frame = self.cap.read()
                if not ret:
                    self.video_finished.emit()
                    break

                # 推理队列投递原始分辨率帧（保证 LPR 识别精度）
                if self.infer_queue.full():
                    try:
                        self.infer_queue.get_nowait()
                    except _queue.Empty:
                        pass
                try:
                    self.infer_queue.put_nowait(frame)   # 不复制，推理线程只读
                except _queue.Full:
                    pass

                # 显示帧在后台线程缩小，大幅减轻主线程渲染压力
                disp = cv2.resize(frame, (self._disp_w, self._disp_h),
                                  interpolation=cv2.INTER_LINEAR)
                self.frame_ready.emit(disp)
                self.msleep(self._interval_ms)
        except Exception as e:
            logger.error(f"读帧线程崩溃: {e}")
            self.video_finished.emit()

    def pause(self)  -> None: self._paused = True
    def resume(self) -> None: self._paused = False

    def stop(self) -> None:
        self._running = False
        self.wait(3000)
        self.cap.release()


# ── 推理线程（YOLO + LPR，独立运行，不阻塞帧显示）────────────────────────────
class InferenceThread(QThread):
    rod_state_ready = pyqtSignal(str)   # YOLO 检测结果
    plate_detected  = pyqtSignal(str)   # 车牌号

    def __init__(
        self,
        infer_queue: "_queue.Queue",
        yolo_model: YOLO,
        plate_catcher: lpr3.LicensePlateCatcher,
        config: dict,
    ):
        super().__init__()
        self.infer_queue   = infer_queue
        self.yolo_model    = yolo_model
        self.plate_catcher = plate_catcher
        self.config        = config
        self._running      = True
        self._fc           = 0   # 推理帧计数

    def run(self) -> None:
        try:
            while self._running:
                try:
                    frame = self.infer_queue.get(timeout=0.15)
                except _queue.Empty:
                    continue

                fc = self._fc
                self._fc += 1

                # YOLO 每帧都跑（GPU 上耗时极短，状态需要实时）
                try:
                    results = self.yolo_model(frame, verbose=False)[0]
                    boxes   = results.boxes.xyxy.tolist()
                    if boxes:
                        cls   = int(results.boxes.cls.tolist()[0])
                        state = "抬起" if cls == 0 else "关闭" if cls == 1 else "车杆损坏"
                    else:
                        state = "未检测到车杆"
                    self.rod_state_ready.emit(state)
                except Exception as e:
                    logger.error(f"YOLO 推理异常（跳过本帧）: {e}")

                # LPR 每 3 推理帧跑一次（CPU，适当降频）
                if fc % 3 == 0:
                    try:
                        threshold   = self.config.get("confidence_threshold", 0.85)
                        lpr_results = self.plate_catcher(frame)
                        for p in sorted(lpr_results, key=lambda x: x[1], reverse=True):
                            plate_no, conf = p[0].upper().strip(), p[1]
                            if conf < threshold:
                                continue
                            self.plate_detected.emit(plate_no)
                            break   # 只取置信度最高的一个
                    except Exception as e:
                        logger.error(f"车牌识别异常（跳过本帧）: {e}")
        except Exception as e:
            logger.error(f"推理线程崩溃: {e}")

    def stop(self) -> None:
        self._running = False
        self.wait(3000)


# ── 数据统计页签 ───────────────────────────────────────────────────────────────
class DataStatsTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.rush_event_data: list = []
        self._log_file_pos: int    = 0    # 已读到的日志文件字节偏移，用于增量追加
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
        """全量加载日志（初始化 / 刷新按钮）。"""
        if not os.path.exists(LOG_PATH):
            self.log_edit.setPlainText("日志文件不存在")
            self._log_file_pos = 0
            return
        try:
            with open(LOG_PATH, "r", encoding="utf-8") as f:
                content = f.read()
                self._log_file_pos = f.tell()
            self.log_edit.setPlainText(content)
            self.log_edit.verticalScrollBar().setValue(
                self.log_edit.verticalScrollBar().maximum()
            )
        except Exception as e:
            self.log_edit.setPlainText(f"读取日志失败: {e}")

    def _append_log(self) -> None:
        """增量追加新日志内容（文件监听触发，避免全量重读）。"""
        if not os.path.exists(LOG_PATH):
            return
        try:
            # 文件被轮转时大小会小于已记录偏移，退回全量加载
            if os.path.getsize(LOG_PATH) < self._log_file_pos:
                self._load_log()
                return
            with open(LOG_PATH, "r", encoding="utf-8") as f:
                f.seek(self._log_file_pos)
                new_content = f.read()
                self._log_file_pos = f.tell()
            if new_content:
                cursor = self.log_edit.textCursor()
                cursor.movePosition(QTextCursor.End)
                cursor.insertText(new_content)
                self.log_edit.verticalScrollBar().setValue(
                    self.log_edit.verticalScrollBar().maximum()
                )
        except Exception as e:
            logger.error(f"增量读取日志失败: {e}")

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
            self._append_log()   # 增量追加，不全量重读
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

        self.config = load_config()
        validate_assets(self.config)   # 缺文件立即弹框退出，不让程序带病运行
        self._audio_ok = True

        # ── YOLO 模型加载（GPU 不可用时自动回退 CPU）──────────────────────────
        try:
            self.yolo_model = YOLO(self.config["model_path"]).to("cuda")
            logger.info("YOLO 模型已加载（CUDA）")
        except Exception as e:
            logger.warning(f"CUDA 不可用，尝试 CPU: {e}")
            try:
                self.yolo_model = YOLO(self.config["model_path"])
                logger.info("YOLO 模型已加载（CPU）")
            except Exception as e2:
                QMessageBox.critical(None, "启动失败", f"无法加载 YOLO 模型:\n{e2}")
                sys.exit(1)

        # ── 车牌识别器 ────────────────────────────────────────────────────────
        try:
            self.plate_catcher = lpr3.LicensePlateCatcher()
        except Exception as e:
            QMessageBox.critical(None, "启动失败", f"无法初始化车牌识别器:\n{e}")
            sys.exit(1)

        # ── 音频（设备不存在时静默禁用，不崩溃）─────────────────────────────
        try:
            pygame.mixer.init()
        except Exception as e:
            logger.warning(f"音频初始化失败，报警音已禁用: {e}")
            self._audio_ok = False

        # ── 检测状态变量 ──────────────────────────────────────────────────────
        self.current_video_path         = ""
        self.passed_plates: list        = []      # [(timestamp, plate_no), ...]
        self._passed_plate_times: dict  = {}      # plate_no -> last_seen float，冷却去重
        self._plate_table_offset: int   = 0       # 表格已移除的头部行数（用于修改车牌时还原真实索引）
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

        self.reader: FrameReaderThread  = None
        self.infer:  InferenceThread    = None
        self._infer_queue: _queue.Queue = None

        # 报警自动清除定时器（3 秒后清除警告文字并停止报警音）
        self._warn_timer = QTimer(self)
        self._warn_timer.setSingleShot(True)
        self._warn_timer.timeout.connect(self._clear_warning)

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
        self._infer_queue = _queue.Queue(maxsize=_INFER_QUEUE_SIZE)
        self.reader = FrameReaderThread(cap, self._infer_queue)
        self.infer  = InferenceThread(self._infer_queue, self.yolo_model, self.plate_catcher, self.config)
        self.reader.frame_ready.connect(self._on_frame_ready)
        self.reader.video_finished.connect(self._on_video_finished)
        self.infer.rod_state_ready.connect(self._on_rod_state)
        self.infer.plate_detected.connect(self._on_plate_detected)
        self.infer.start()
        self.reader.start()
        self.btn_pause.setEnabled(True)
        self.btn_pause.setText("⏸ 暂停")

    def _reset_state(self) -> None:
        if self.reader is not None:
            self.reader.stop()   # 先停读帧（断绝推理队列来源）
            self.reader = None
        if self.infer is not None:
            self.infer.stop()    # 再停推理（等待当前帧处理完毕）
            self.infer = None
        self._infer_queue = None

        self.passed_plates            = []
        self._passed_plate_times      = {}
        self._plate_table_offset      = 0
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
        self._warn_timer.stop()
        self.warning_label.clear()
        self.btn_pause.setEnabled(False)

    def toggle_pause(self) -> None:
        if self.reader is None:
            return
        if self.reader._paused:
            self.reader.resume()
            self.btn_pause.setText("⏸ 暂停")
        else:
            self.reader.pause()
            self.btn_pause.setText("▶️ 继续")

    # ── 信号槽（主线程执行）──────────────────────────────────────────────────
    def _on_frame_ready(self, frame: np.ndarray) -> None:
        """读帧线程信号：仅负责将帧渲染到 UI，不做任何推理。"""
        self.latest_frame = frame
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb.shape
        # QPixmap.fromImage 内部会复制数据，无需额外 .copy()；FastTransformation 速度约 10x
        q_img  = QImage(rgb.data, w, h, ch * w, QImage.Format_RGB888)
        pixmap = QPixmap.fromImage(q_img).scaled(
            self.video_label.size(), Qt.KeepAspectRatio, Qt.FastTransformation
        )
        self.video_label.setPixmap(pixmap)

    def _on_rod_state(self, rod_state: str) -> None:
        """推理线程信号：处理车杆状态变化和冲岗逻辑。"""
        self.status_label.setText(f"车杆状态：{rod_state}")
        if rod_state != self.previous_rod_state:
            self._handle_rod_state_change(rod_state)
        self._handle_rod_state_no_change(rod_state)
        self.previous_rod_state = rod_state

    def _on_plate_detected(self, plate_no: str) -> None:
        if not plate_no:
            return

        # 时间冷却去重：同一车牌在 _PLATE_COOLDOWN_SEC 内不重复记录
        now = time.time()
        if now - self._passed_plate_times.get(plate_no, 0) < _PLATE_COOLDOWN_SEC:
            return

        # Levenshtein 去抖：与上一辆高度相似则认为是同一张车牌的 OCR 抖动，丢弃
        if self.previous_plate and 0 < Levenshtein.distance(plate_no, self.previous_plate) < 3:
            logger.debug(f"去抖丢弃: {plate_no}（上一辆: {self.previous_plate}）")
            return

        # 确认为新车辆
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.passed_plates.append((timestamp, plate_no))
        self._passed_plate_times[plate_no] = now
        self.vehicle_count += 1
        self.previous_plate = plate_no
        self.current_plate  = plate_no
        self.count_label.setText(f"通过车辆数目：{self.vehicle_count}")

        # 实时写库（不再等到视频结束批量写入）
        try:
            insert_plate_record(self.current_video_path, timestamp, plate_no)
        except Exception as e:
            logger.error(f"车牌实时写库失败: {e}")

        # 更新过车表格（超过上限时移除最旧一行，并记录偏移量用于车牌修改）
        if self.plate_table.rowCount() >= _PLATE_TABLE_MAX_ROWS:
            self.plate_table.removeRow(0)
            self._plate_table_offset += 1
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
            # 新一轮开始：清空本轮车牌列表，重置跟车标志
            self.current_up_plates = []
            self.tailgate_recorded = False
            logger.info("车杆抬起，开始本轮冲岗检测")
        elif state == "关闭":
            # 本轮通行结束：重置全部状态，准备下一轮
            self.current_up_plates      = []
            self.tailgate_recorded      = False
            self.barrier_crash_recorded = False
            self._warn_timer.stop()
            self.warning_label.clear()
            if self._audio_ok:
                try:
                    pygame.mixer.music.stop()
                except Exception:
                    pass
            logger.info("车杆关闭，已重置冲岗判断状态")
        elif state in ("车杆损坏", "未检测到车杆"):
            # YOLO 检测到车杆损坏，或车杆从视野中消失 → 均为撞杆冲岗
            if self.current_plate and not self.barrier_crash_recorded:
                self._show_warning(f"警告：撞杆冲岗\n可疑车牌：{self.current_plate}")
                self._play_alert()
                self._record_rush_event(
                    datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                    self.current_plate,
                    "撞杆冲岗",
                )
                logger.warning(f"撞杆冲岗，可疑车牌: {self.current_plate}")
                self.barrier_crash_recorded = True

    def _handle_rod_state_no_change(self, state: str) -> None:
        """每次 YOLO 检测完毕都调用（含状态变化的那一帧）。"""
        # 只在车杆抬起期间做跟车冲岗检测
        if state == "抬起":
            self._check_for_rush()

    def _check_for_rush(self) -> bool:
        """检测跟车冲岗：本次开闸内出现超过 1 辆车。"""
        if not self.current_plate:          # 修复：空车牌不追加
            return False
        if self.current_plate not in self.current_up_plates:
            self.current_up_plates.append(self.current_plate)
        if len(self.current_up_plates) > 1 and self.vehicle_count > 1:
            if not self.tailgate_recorded:
                self._show_warning(f"警告：跟车冲岗\n可疑车牌：{self.current_plate}")
                self._play_alert()
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
            safe_plate = "".join(c for c in plate_no if c.isalnum() or c in "-_")
            filename   = f"{timestamp.replace(':', '-')}_{safe_plate}.jpg"
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
            except Exception as e:
                logger.warning(f"冲岗 JSON 读取失败，将覆盖写入: {e}")
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

        # 更新内存过车列表（row 是表格可见行号，需加偏移量还原在 passed_plates 中的真实下标）
        actual_idx = row + self._plate_table_offset
        if 0 <= actual_idx < len(self.passed_plates):
            ts = self.passed_plates[actual_idx][0]
            self.passed_plates[actual_idx] = (ts, new_plate)
            t = self._passed_plate_times.pop(old_plate, time.time())
            self._passed_plate_times[new_plate] = t
            if self.current_plate  == old_plate: self.current_plate  = new_plate
            if self.previous_plate == old_plate: self.previous_plate = new_plate

            # 更新数据库
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

    def _show_warning(self, text: str) -> None:
        """显示报警文字并启动 3 秒自动清除定时器。"""
        self.warning_label.setText(text)
        self._warn_timer.start(3000)

    def _clear_warning(self) -> None:
        """定时器到期：清除报警文字，停止报警音。"""
        self.warning_label.clear()
        if self._audio_ok:
            try:
                pygame.mixer.music.stop()
            except Exception:
                pass

    def _play_alert(self) -> None:
        if not self._audio_ok:
            return
        try:
            if not pygame.mixer.music.get_busy():
                pygame.mixer.music.load(self.config["alert_sound"])
                pygame.mixer.music.play()
        except Exception as e:
            logger.error(f"播放报警音失败: {e}")

    def closeEvent(self, event) -> None:
        if self.reader is not None:
            self.reader.stop()
        if self.infer is not None:
            self.infer.stop()
        pygame.mixer.music.stop()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
