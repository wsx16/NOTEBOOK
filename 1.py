import sys
import cv2
import numpy as np
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QLabel, QVBoxLayout, QHBoxLayout,
    QTableWidget, QTableWidgetItem, QPushButton, QFileDialog, QMessageBox,
    QInputDialog, QTabWidget, QTextEdit, QDialog
)
from PyQt5.QtCore import Qt, QTimer, QFileSystemWatcher
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
from PyQt5.QtWidgets import QListWidget, QListWidgetItem
warnings.filterwarnings('ignore', category=DeprecationWarning)

# 在配置部分新增冲岗记录路径
RUSH_EVENT_DIR = "rush_events"
RUSH_EVENT_RECORD = os.path.join(RUSH_EVENT_DIR, "rush_records.json")
# 配置文件路径
CONFIG_PATH = "config.json"
# 日志文件路径
LOG_PATH = "system.log"
# 车牌记录文件路径
PLATE_RECORD_PATH = "plate_records.txt"

# 加载配置文件
def load_config():
    try:
        with open(CONFIG_PATH, "r") as f:
            config = json.load(f)
            logger.info("配置文件加载成功")
            return config
    except FileNotFoundError:
        logger.warning("配置文件未找到，使用默认配置")
        return {
            "model_path": "best.pt",
            "font_path": "simhei.ttf",
            "confidence_threshold": 0.975,
            "alert_sound": "alert.wav"
        }
    except Exception as e:
        logger.error(f"加载配置文件失败: {e}", exc_info=True)
        return {}

def setup_logging():
    # 创建日志记录器
    logger = logging.getLogger("SystemLogger")
    logger.setLevel(logging.DEBUG)  # 设置最低日志级别

    # 避免重复添加处理器
    if logger.handlers:
        return logger

    logger.propagate = False

    try:
        # 创建文件处理器，按天轮转
        file_handler = TimedRotatingFileHandler(
            LOG_PATH,
            when="midnight",
            interval=1,
            backupCount=7,
            encoding="utf-8",
            delay=True
        )
        file_handler.suffix = "%Y-%m-%d"
        file_handler.setLevel(logging.DEBUG)

        # 创建控制台处理器
        console_handler = logging.StreamHandler()
        console_handler.setLevel(logging.INFO)

        # 创建日志格式
        log_format = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
        file_handler.setFormatter(log_format)
        console_handler.setFormatter(log_format)

        # 添加处理器到记录器
        logger.addHandler(file_handler)
        logger.addHandler(console_handler)

    except Exception as e:
        print(f"无法设置日志处理器: {e}")

    return logger

logger = setup_logging()

# 保存配置文件
def save_config(config):
    with open(CONFIG_PATH, "w", encoding='utf-8') as f:
        json.dump(config, f, indent=4)

# 绘制中文文本
def draw_chinese_text_pil(image, text, position, font_path, font_size=30, color=(0, 255, 0)):
    image_pil = Image.fromarray(cv2.cvtColor(image, cv2.COLOR_BGR2RGB))
    draw = ImageDraw.Draw(image_pil)
    font = ImageFont.truetype(font_path, font_size)
    draw.text(position, text, font=font, fill=color)
    return cv2.cvtColor(np.array(image_pil), cv2.COLOR_RGB2BGR)


# DataStatsTab 类的完整实现
class DataStatsTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.file_watcher = QFileSystemWatcher()
        self.file_watcher.fileChanged.connect(self.update_content)
        self.init_ui()
        self.load_rush_events()
        self.file_watcher.addPath(RUSH_EVENT_RECORD)

    def init_ui(self):
        layout = QVBoxLayout()
        self.tabs = QTabWidget()

        # 车牌记录
        self.plate_edit = QTextEdit()  # 更改为可编辑的QTextEdit
        self.tabs.addTab(self.plate_edit, "车牌记录")

        # 系统日志 - 现在也可以编辑
        self.log_edit = QTextEdit()  # 更改为可编辑的QTextEdit
        self.tabs.addTab(self.log_edit, "系统日志")

        # 配置文件 - 现在也可以编辑
        self.config_edit = QTextEdit()  # 更改为可编辑的QTextEdit
        self.tabs.addTab(self.config_edit, "配置文件")

        # 冲岗情况
        self.rush_list = QListWidget()
        self.rush_list.itemDoubleClicked.connect(self.show_rush_details)
        self.tabs.addTab(self.rush_list, "冲岗情况")

        self.file_watcher.addPaths([PLATE_RECORD_PATH, LOG_PATH, CONFIG_PATH, RUSH_EVENT_RECORD])

        # 添加保存按钮
        self.btn_save_plate = QPushButton("保存车牌记录修改")
        self.btn_save_log = QPushButton("保存系统日志修改")
        self.btn_save_config = QPushButton("保存配置文件修改")
        self.btn_refresh = QPushButton("刷新数据")

        self.btn_refresh.clicked.connect(self.load_all_files)
        self.btn_save_plate.clicked.connect(lambda: self.save_changes(PLATE_RECORD_PATH, self.plate_edit))
        self.btn_save_log.clicked.connect(lambda: self.save_changes(LOG_PATH, self.log_edit))
        self.btn_save_config.clicked.connect(lambda: self.save_changes(CONFIG_PATH, self.config_edit))

        button_layout = QHBoxLayout()
        button_layout.addWidget(self.btn_save_plate)
        button_layout.addWidget(self.btn_save_log)
        button_layout.addWidget(self.btn_save_config)
        button_layout.addWidget(self.btn_refresh)

        layout.addWidget(self.tabs)
        layout.addLayout(button_layout)  # 添加到布局中
        self.setLayout(layout)

        # 初始化文件监控
        self.file_watcher.addPaths([PLATE_RECORD_PATH, LOG_PATH, CONFIG_PATH])
        self.load_all_files()

    def show_rush_details(self, item):
            index = self.rush_list.row(item)
            with open(RUSH_EVENT_RECORD, "r", encoding="utf-8") as f:
                events = json.load(f)
                event = events[index]

                detail_dialog = QDialog()
                detail_dialog.setWindowTitle("冲岗事件详情")
                layout = QVBoxLayout()

                # 显示大图
                img_label = QLabel()
                pixmap = QPixmap(event["image_path"])
                img_label.setPixmap(pixmap.scaled(1500, 1400, Qt.KeepAspectRatio))

                # 详细信息
                info_label = QLabel(
                    f"发生时间：{event['timestamp']}\n"
                    f"车牌号码：{event['plate']}\n"
                    f"事件类型：{event['warning_type']}\n"
                    f"原始警告信息：{event['message']}"
                )

                layout.addWidget(img_label)
                layout.addWidget(info_label)
                detail_dialog.setLayout(layout)
                detail_dialog.exec_()

    def load_rush_events(self):
        self.rush_list.clear()
        if os.path.exists(RUSH_EVENT_RECORD):
            try:
                with open(RUSH_EVENT_RECORD, "r", encoding="utf-8") as f:
                    events = json.load(f)
                    for event in events:
                        item = QListWidgetItem()
                        widget = QWidget()
                        layout = QHBoxLayout()

                        # 缩略图
                        img_label = QLabel()
                        pixmap = QPixmap(event["image_path"])
                        img_label.setPixmap(pixmap.scaled(100, 100, Qt.KeepAspectRatio))

                        # 事件信息
                        info_label = QLabel(
                            f"时间：{event['timestamp']}\n"
                            f"车牌：{event['plate']}\n"
                            f"类型：{event['warning_type']}"
                        )

                        layout.addWidget(img_label)
                        layout.addWidget(info_label)
                        widget.setLayout(layout)
                        item.setSizeHint(widget.sizeHint())

                        self.rush_list.addItem(item)
                        self.rush_list.setItemWidget(item, widget)
            except Exception as e:
                QMessageBox.critical(self, "错误", f"加载冲岗记录失败：{str(e)}")

    def load_all_files(self):
        self.load_file_content(PLATE_RECORD_PATH, self.plate_edit)
        self.load_file_content(LOG_PATH, self.log_edit)
        self.load_config_file()

    def load_file_content(self, file_path, editor):
        if os.path.exists(file_path):
            try:
                with open(file_path, "r", encoding="utf-8") as f:
                    content = f.read()
                    editor.setPlainText(content)
                    editor.verticalScrollBar().setValue(editor.verticalScrollBar().maximum())
            except Exception as e:
                editor.setPlainText(f"读取文件错误: {str(e)}")
        else:
            editor.setPlainText(f"文件不存在: {file_path}")

    def load_config_file(self):
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                content = json.dumps(json.load(f), indent=4, ensure_ascii=False)
                self.config_edit.setPlainText(content)
        except Exception as e:
            self.config_edit.setPlainText(f"加载配置文件失败: {str(e)}")

    def update_content(self, path):
        if path == RUSH_EVENT_RECORD:
            self.load_rush_events()
        elif path == PLATE_RECORD_PATH:
            self.load_file_content(path, self.plate_edit)
        elif path == LOG_PATH:
            self.load_file_content(path, self.log_edit)
        elif path == CONFIG_PATH:
            self.load_config_file()

    def save_changes(self, file_path, editor):
        """保存指定编辑器的内容"""
        try:
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(editor.toPlainText())
            QMessageBox.information(self, "提示", f"{file_path} 修改已保存！")
        except Exception as e:
            QMessageBox.critical(self, "错误", f"保存文件时出错: {str(e)}")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("基于机器视觉的高速收费站卡口冲岗行为识别系统")
        self.setFixedSize(1800, 1188)
        os.makedirs(RUSH_EVENT_DIR, exist_ok=True)

        # 初始化组件
        self.btn_open = QPushButton("📁选择视频文件")
        self.btn_open.setFixedSize(350, 45)
        self.btn_pause = QPushButton("⏸暂停")
        self.btn_pause.setFixedSize(350, 45)
        self.status_label = QLabel("车杆状态：未检测到车杆")
        self.count_label = QLabel("通过车辆数目：0")
        self.btn_modify = QPushButton("修改车牌")
        self.video_label = QLabel()
        self.warning_label = QLabel()
        self.plate_table = QTableWidget()
        self.plate_table.setColumnCount(3)
        self.plate_table.setHorizontalHeaderLabels(["序号", "实时时间", "车牌号"])
        self.plate_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.plate_table.setColumnWidth(0, 100)
        self.plate_table.setColumnWidth(1, 200)
        self.plate_table.setColumnWidth(2, 120)

        self.rush_table = QTableWidget()
        self.rush_table.setColumnCount(3)
        self.rush_table.setHorizontalHeaderLabels(["时间", "车牌号", "警告信息"])
        self.rush_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.rush_table.setColumnWidth(0, 200)
        self.rush_table.setColumnWidth(1, 120)
        self.rush_table.setColumnWidth(2, 100)

        # 加载配置
        self.config = load_config()
        self.yolo_model = YOLO(self.config["model_path"]).to("cuda")
        self.plate_catcher = lpr3.LicensePlateCatcher()
        self.cap = None
        self.is_paused = False

        # 初始化状态变量
        self.previous_rod_state = None
        self.current_up_plates = []
        self.warning_message = ""
        self.passed_plates = []
        self.vehicle_count = 0
        self.previous_plate = None
        self.current_video_path = ""
        self.current_plate = ''
        self.rush_events = set()  # 使用集合来存储唯一的车牌号和警告信息
        self.a = False
        self.b = False
        self.frame_count = 0

        # 初始化UI
        self.init_ui()
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_frame)

        # 初始化声音报警
        pygame.mixer.init()

    def init_ui(self):
        # 设置主窗口背景
        self.setStyleSheet(f"""
               QMainWindow {{
                   background-image: url(background.png);
                   background-position: center;
                   background-repeat: no-repeat;
                   background-size: cover;
               }}
                /* 主内容区域透明 */
                QWidget#mainWidget {{
                    background: transparent;
                    border: none;
                }}
               QPushButton {{
                   background: rgba(245, 245, 245, 0.8);
                   border: 1px solid #B0BEC5;
                   border-radius: 5px;
                   padding: 8px 15px;
                   min-width: 100px;
                   font: 14px '微软雅黑';
                   color: #2C3E50;
               }}
               QPushButton:hover {{
                   background: rgba(220, 220, 220, 0.9);
                   border: 1px solid #90A4AE;
               }}
               QPushButton:pressed {{
                   background: rgba(200, 200, 200, 0.9);
                   border: 1px solid #78909C;
               }}
               QPushButton:disabled {{
                   background: rgba(220, 220, 220, 0.6);
                   color: #78909C;
               }}
               QLabel#titleLabel {{
                   background: transparent;
               }}
           """)

        main_widget = QWidget()
        main_widget.setObjectName("mainWidget")
        main_layout = QVBoxLayout()
        main_widget.setLayout(main_layout)
        self.setCentralWidget(main_widget)

        # 修改标题标签
        title_label = QLabel("高速收费站卡口冲岗行为识别系统")
        title_label.setObjectName("titleLabel")  # 添加标识用于样式控制
        title_label.setAlignment(Qt.AlignCenter)
        title_label.setStyleSheet("""
            QLabel {
                font: bold 36px '微软雅黑';
                color: #2C3E50;
                padding: 20px;
                border-bottom: 3px solid #3498DB;
                background: rgba(255, 255, 255, 0.1);
            }
        """)
        main_layout.addWidget(title_label)
        # 主分页容器
        self.tab_widget = QTabWidget()
        self.tab_widget.setStyleSheet("""
            QTabWidget::pane {
                border: 1px solid #B0BEC5;
                background: rgba(255, 255, 255, 0);
            }
            QTabBar::tab {
                background: #CFD8DC;
                padding: 10px;
                border: 1px solid #B0BEC5;
                border-radius: 4px;
            }
            QTabBar::tab:selected {
                background: #B0BEC5;
            }
            QTabBar::tab:hover {
                background: #cccccc;
            }
        """)
        main_layout.addWidget(self.tab_widget)  # 将分页容器添加到标题下方

        # 视频检测分页
        video_tab = QWidget()
        self.setup_video_tab(video_tab)
        self.tab_widget.addTab(video_tab, "视频检测")

        # 数据统计分页
        stats_tab = DataStatsTab()
        self.tab_widget.addTab(stats_tab, "数据统计")

        # 设置布局间距
        main_layout.setSpacing(20)
        main_layout.setContentsMargins(20, 20, 20, 20)  # 上下左右边距

    def setup_video_tab(self, tab):
        main_layout = QHBoxLayout(tab)

        # 视频显示区域
        self.video_label.setAlignment(Qt.AlignCenter)
        self.video_label.setMaximumSize(1200, 1000)
        main_layout.addWidget(self.video_label)

        # 右侧信息区域
        info_widget = QWidget()
        info_layout = QVBoxLayout()
        info_layout.setAlignment(Qt.AlignHCenter)
        info_widget.setLayout(info_layout)
        info_widget.setMaximumWidth(500)  # 扩大右侧信息区域宽度
        main_layout.addWidget(info_widget)

        # 文件操作区
        file_widget = QWidget()
        self.btn_open.clicked.connect(self.open_file_dialog)
        self.btn_pause.clicked.connect(self.toggle_pause)
        self.btn_pause.setEnabled(False)
        info_layout.addWidget(self.btn_open)
        info_layout.addWidget(self.btn_pause)

        info_layout.addWidget(file_widget)

        # 状态显示
        info_layout.addWidget(self.status_label)
        info_layout.addWidget(self.count_label)
        self.status_label.setStyleSheet("""
            QLabel {
                font: bold 20px '微软雅黑';
                color: #2C3E50;
                padding: 12px;
                border-bottom: 3px solid #3498DB;
                background: rgba(255, 255, 255, 0.5);
            }
        """)
        self.count_label.setStyleSheet("""
            QLabel {
                font: bold 20px '微软雅黑';
                color: #2C3E50;
                padding: 12px;
                border-bottom: 3px solid #3498DB;
                background: rgba(255, 255, 255, 0.5);
            }
        """)

        # 报警信息
        self.warning_label.setFixedHeight(60)
        self.warning_label.setStyleSheet("color: red; font-size: 24px; font-weight: bold;")
        info_layout.addWidget(self.warning_label)

        # 车牌表格
        plate_container = QWidget()
        plate_layout = QVBoxLayout()
        plate_container.setLayout(plate_layout)
        plate_title = QLabel("通过车辆车牌号")
        plate_title.setStyleSheet("font-size: 25px; "
                                  "font-family: '微软雅黑';"
                                  "font-weight: bold;"
                                  "background: rgba(0, 255, 255, 0.3); "
                                  "border-bottom: 3px solid #3498DB;")
        plate_layout.addWidget(plate_title)
        plate_layout.addWidget(self.plate_table)

        # 添加修改按钮
        self.btn_modify.setStyleSheet("font-size: 16px; padding: 8px;")
        self.btn_modify.clicked.connect(self.modify_plate_number)
        plate_layout.addWidget(self.btn_modify)

        plate_container.setFixedWidth(500)
        info_layout.addWidget(plate_container)

        # 冲岗事件表格
        rush_event_container = QWidget()
        rush_event_layout = QVBoxLayout()
        rush_event_container.setLayout(rush_event_layout)
        rush_event_title = QLabel("冲岗车辆记录")
        rush_event_title.setStyleSheet("font-size: 25px; "
                                       "font-family: '微软雅黑'; "
                                       "font-weight: bold; "
                                       "background: rgba(0, 255, 255, 0.3); "
                                       "border-bottom: 3px solid #3498DB;")
        rush_event_layout.addWidget(rush_event_title)
        rush_event_layout.addWidget(self.rush_table)

        rush_event_container.setFixedWidth(500)
        info_layout.addWidget(rush_event_container)

    # 修改车牌号功能
    def modify_plate_number(self):
        selected_items = self.plate_table.selectedItems()
        if not selected_items or len(selected_items) != 3:
            QMessageBox.warning(self, "警告", "请先选择一行要修改的车牌号！")
            return

        row = selected_items[0].row()
        old_plate = selected_items[2].text()
        new_plate, ok = QInputDialog.getText(
            self, "修改车牌号",
            "请输入新的车牌号：",
            text=old_plate
        )

        if ok and new_plate:
            new_plate = new_plate.strip().upper()
            # 更新表格显示
            self.plate_table.setItem(row, 2, QTableWidgetItem(new_plate))
            # 更新存储的车牌记录
            if 0 <= row < len(self.passed_plates):
                # 更新所有相关记录
                old_value = self.passed_plates[row][1]  # 获取旧车牌号
                self.passed_plates[row] = (self.passed_plates[row][0], new_plate)  # 更新为元组形式

                # 更新当前显示的车牌信息
                if self.current_plate == old_value:
                    self.current_plate = new_plate
                if self.previous_plate == old_value:
                    self.previous_plate = new_plate

                # 更新警告信息中的车牌号
                for event in self.rush_events:
                    if old_value in event:
                        self.rush_events.remove(event)
                        self.rush_events.add((event[0], new_plate, event[2]))
                        break

                with open(PLATE_RECORD_PATH, "a", encoding="utf-8") as f:
                    f.write(f"车牌号被修改\n")
                    for time, plate in self.passed_plates:
                        f.write(f"{time} - {plate}\n")
                logger.info(f"车牌已保存到文件：{PLATE_RECORD_PATH}")
                logger.info(f"车牌号修改成功：{old_value} -> {new_plate}")
                QMessageBox.information(self, "提示", "车牌号修改成功！")

    def toggle_pause(self):
        self.is_paused = not self.is_paused
        self.btn_pause.setText("▶️继续" if self.is_paused else "⏸暂停")
        if not self.is_paused and self.cap is not None:
            self.timer.start(30)

    def open_file_dialog(self):
        self.reset_status()
        file_path, _ = QFileDialog.getOpenFileName(
            self, "选择视频文件", "",
            "视频文件 (*.mp4 *.avi *.mov);;所有文件 (*)"
        )
        if file_path:
            self.load_video(file_path)

    def load_video(self, file_path):
        try:
            self.current_video_path = file_path
            logger.info(f"视频文件: {os.path.basename(self.current_video_path)}")
            self.cap = cv2.VideoCapture(file_path)
            if not self.cap.isOpened():
                raise ValueError("无法打开视频文件")
            self.btn_pause.setEnabled(True)
            self.is_paused = False
            self.btn_pause.setText("⏸暂停")
            self.timer.start(30)
        except Exception as e:
            QMessageBox.critical(self, "错误", f"加载视频失败：{str(e)}")
            logger.error(f"加载视频失败：{str(e)}")

    def reset_status(self):
        self.passed_plates = []
        self.vehicle_count = 0
        self.plate_table.setRowCount(0)
        self.rush_table.setRowCount(0)
        self.previous_rod_state = None
        self.current_up_plates = []
        self.warning_message = ""
        self.previous_plate = None
        self.current_plate = ''
        self.rush_events.clear()
        self.a = False
        self.b = False
        self.frame_count = 0

        self.status_label.setText("车杆状态：未检测到车杆")
        self.count_label.setText("通过车辆数目：0")
        self.warning_label.clear()

        if self.timer.isActive():
            self.timer.stop()
        if self.cap is not None:
            self.cap.release()
            self.cap = None

    def update_frame(self):
        if self.is_paused or self.cap is None or not self.cap.isOpened():
            return
        ret, frame = self.cap.read()
        if not ret:
            logger.info("视频播放完毕")
            self.timer.stop()
            self.save_plates_to_file()
            QMessageBox.information(self, "提示", "视频播放完毕")
            self.btn_pause.setEnabled(False)
            return

        # 每10帧进行一次车杆状态检测
        if self.frame_count % 10 == 0:
            yolo_results = self.yolo_model(frame)[0]
            rod_boxes = yolo_results.boxes.xyxy.tolist()
            current_rod_state = "未检测到车杆"
            if rod_boxes:
                cls = int(yolo_results.boxes.cls.tolist()[0])
                current_rod_state = "抬起" if cls == 0 else "关闭" if cls == 1 else "车杆损坏" if cls == 2 else "未检测到车杆"
            self.status_label.setText(f"车杆状态：{current_rod_state}")

            # 状态变化稳定性判断
            if self.previous_rod_state != current_rod_state:
                self.handle_rod_state_change(current_rod_state)

            self.previous_rod_state = current_rod_state
            if self.previous_rod_state == current_rod_state:
                self.handle_rod_state_no_change(current_rod_state)

        # 每7帧进行一次车牌识别
        if self.frame_count % 7 == 0:
            lpr_results = self.plate_catcher(frame)
            for plate_info in sorted(lpr_results, key=lambda x: x[1], reverse=True):
                plate_no = plate_info[0].upper().strip()
                plate_conf = plate_info[1]
                if plate_conf < self.config["confidence_threshold"]:
                    continue
                x1, y1, x2, y2 = map(int, plate_info[3])
                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 0, 255), 2)
                frame = draw_chinese_text_pil(frame, plate_no, (x1, y1 - 30), self.config["font_path"], 30, (0, 0, 255))

                if plate_no not in [p[1] for p in self.passed_plates] and plate_no != self.previous_plate:
                    self.update_plate_info(plate_no)
                    logger.info(f"检测到新车牌: {plate_no}")

        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = frame.shape
        bytes_per_line = ch * w
        q_img = QImage(frame.data, w, h, bytes_per_line, QImage.Format_RGB888)
        pixmap = QPixmap.fromImage(q_img).scaled(self.video_label.size(), Qt.KeepAspectRatio)
        self.video_label.setPixmap(pixmap)
        self.frame_count += 1

    def update_plate_info(self, plate_no):
        self.vehicle_count += 1
        distance = self.calculate_levenshtein_distance(plate_no)
        if 0 < distance < 3:
            self.handle_plate_duplicate()
        self.count_label.setText(f"通过车辆数目：{self.vehicle_count}")
        self.previous_plate = plate_no
        self.current_plate = plate_no
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.passed_plates.append((timestamp, plate_no))  # 存储车牌号及其时间戳
        row_position = self.plate_table.rowCount()
        self.plate_table.insertRow(row_position)
        self.plate_table.setItem(row_position, 0, QTableWidgetItem(str(self.vehicle_count)))
        self.plate_table.setItem(row_position, 1, QTableWidgetItem(timestamp))
        self.plate_table.setItem(row_position, 2, QTableWidgetItem(plate_no))

    def calculate_levenshtein_distance(self, plate_no):
        if self.previous_plate:
            return Levenshtein.distance(plate_no, self.previous_plate)
        else:
            return 0

    def handle_plate_duplicate(self):
        del self.passed_plates[-1]
        self.vehicle_count -= 1
        self.plate_table.removeRow(self.plate_table.rowCount() - 1)

    def handle_rod_state_change(self, current_rod_state):
        if current_rod_state == "抬起":
            self.current_up_plates = []
            logger.info("车杆抬起")
        if not self.check_for_rush():
            self.warning_message = ""
        if current_rod_state == "关闭":
            logger.info("车杆关闭")

    def handle_rod_state_no_change(self, current_rod_state):
        if current_rod_state == "车杆损坏":
            if len(self.passed_plates) > 0 and not self.check_for_rush():
                self.play_alert_sound()
                self.warning_message = f"警告：撞杆冲岗 \n可疑车牌：{self.current_plate}"
                self.warning_label.setText(self.warning_message)
                if not self.a:
                    self.record_rush_event(datetime.now().strftime("%Y-%m-%d %H:%M:%S"), self.current_plate, self.warning_message)
                    logger.warning(f"撞杆冲岗，可疑车牌: {self.current_plate}")
                    self.a = True
        if current_rod_state in ["抬起", "关闭"] and not self.check_for_rush():
            self.warning_message = ""

    def record_rush_event(self, timestamp, plate_no, warning_message):
        # 保存当前帧图片
        ret, frame = self.cap.read()
        if ret:
            # 生成唯一文件名
                filename = f"{timestamp.replace(':', '-')}_{plate_no}.jpg"
                image_path = os.path.join(RUSH_EVENT_DIR, filename)
                cv2.imwrite(image_path, frame)

                # 确定警告类型
                warning_type = "撞杆冲岗" if "撞杆冲岗" in warning_message else "跟车冲岗"

                # 更新记录文件
                event = {
                    "timestamp": timestamp,
                    "plate": plate_no,
                    "message": warning_message,
                    "warning_type": warning_type,
                    "image_path": image_path
                }

                events = []
                if os.path.exists(RUSH_EVENT_RECORD):
                    with open(RUSH_EVENT_RECORD, "r", encoding="utf-8") as f:
                        events = json.load(f)
                events.append(event)

                with open(RUSH_EVENT_RECORD, "w", encoding="utf-8") as f:
                    json.dump(events, f, ensure_ascii=False, indent=2)

                # 更新表格
                row_position = self.rush_table.rowCount()
                self.rush_table.insertRow(row_position)
                self.rush_table.setItem(row_position, 0, QTableWidgetItem(timestamp))
                self.rush_table.setItem(row_position, 1, QTableWidgetItem(plate_no))
                self.rush_table.setItem(row_position, 2, QTableWidgetItem(warning_type))

        return True

    def save_plates_to_file(self):
        try:
            with open(PLATE_RECORD_PATH, "a", encoding="utf-8") as f:
                f.write(f"\n=== 视频文件: {os.path.basename(self.current_video_path)} ===\n")
                for timestamp, plate in self.passed_plates:
                    f.write(f"{timestamp} - {plate}\n")
            logger.info(f"车牌已保存到文件：{PLATE_RECORD_PATH}\n---------------------------------------------------------------------")
        except Exception as e:
            logger.error(f"保存车牌到文件失败：{str(e)}")

    def check_for_rush(self):
        if self.current_plate not in self.current_up_plates:
            self.current_up_plates.append(self.current_plate)
        if len(self.current_up_plates) > 1 and self.vehicle_count != 1:
            self.warning_message = f"警告：检测到跟车冲岗！\n可疑车牌：{self.current_plate}"
            self.warning_label.setText(self.warning_message)
            self.play_alert_sound()
            if not self.b:
                self.record_rush_event(datetime.now().strftime("%Y-%m-%d %H:%M:%S"), self.current_plate, self.warning_message)
                logger.warning(f"跟车冲岗，可疑车牌: {self.current_plate}")
                self.b = True
            return True

    def play_alert_sound(self):
        try:
            pygame.mixer.music.load(self.config["alert_sound"])
            pygame.mixer.music.play()
        except Exception as e:
            logger.error(f"播放报警声音失败：{str(e)}")

    def closeEvent(self, event):
        if self.cap is not None:
            self.cap.release()
        pygame.mixer.music.stop()  # 停止播放警报声音
        self.timer.stop()  # 停止计时器
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
