"""
历史数据迁移脚本：将 plate_records.txt 和 rush_records.json 导入 SQLite。
可重复执行，INSERT OR IGNORE 保证幂等性。
"""
import json
import os
import re
from collections import defaultdict
from typing import Dict, List, Tuple

from db import DB_PATH, init_db, insert_plate_records, insert_rush_event


PLATE_RECORD_PATH = "plate_records.txt"
RUSH_EVENT_RECORD = os.path.join("rush_events", "rush_records.json")

VIDEO_LINE_PATTERN = re.compile(r"^=== 视频文件:\s*(.*?)\s*===$")
PLATE_LINE_PATTERN = re.compile(r"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\s*-\s*(.+)$")


def parse_plate_records(path: str) -> List[Tuple[str, str, str]]:
    """返回 [(video_name, timestamp, plate_no), ...]"""
    rows: List[Tuple[str, str, str]] = []
    current_video = ""
    if not os.path.exists(path):
        return rows
    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line:
                continue
            m = VIDEO_LINE_PATTERN.match(line)
            if m:
                current_video = m.group(1)
                continue
            if line in ("车牌号被修改",) or line.startswith("[车牌修改]"):
                continue
            m = PLATE_LINE_PATTERN.match(line)
            if m:
                rows.append((current_video, m.group(1), m.group(2).strip()))
    return rows


def parse_rush_events(path: str) -> List[Dict[str, str]]:
    if not os.path.exists(path):
        return []
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return [
        {
            "timestamp":    item.get("timestamp", ""),
            "plate_no":     item.get("plate", ""),
            "warning_type": item.get("warning_type", ""),
            "message":      item.get("message", ""),
            "image_path":   item.get("image_path", ""),
            "video_name":   item.get("video_name", ""),
        }
        for item in data
    ]


def migrate() -> None:
    init_db()  # 建表 + 唯一索引，INSERT OR IGNORE 自动去重

    plate_rows = parse_plate_records(PLATE_RECORD_PATH)
    rush_rows  = parse_rush_events(RUSH_EVENT_RECORD)

    # 按视频名分组批量写入过车记录
    by_video: dict = defaultdict(list)
    for video_name, timestamp, plate_no in plate_rows:
        by_video[video_name].append((timestamp, plate_no))

    for video_name, records in by_video.items():
        insert_plate_records(video_name, records)

    # 逐条写入冲岗事件
    for item in rush_rows:
        event = {
            "timestamp":    item["timestamp"],
            "plate":        item["plate_no"],
            "warning_type": item["warning_type"],
            "message":      item["message"],
            "image_path":   item["image_path"],
        }
        insert_rush_event(event, item["video_name"])

    print(f"plate_records 解析: {len(plate_rows)} 条（重复自动忽略）")
    print(f"rush_events   解析: {len(rush_rows)} 条（重复自动忽略）")


if __name__ == "__main__":
    migrate()
