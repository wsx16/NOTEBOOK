import os
import sqlite3
from contextlib import closing
from typing import Dict, List, Optional, Sequence, Tuple


DB_PATH = "system_data.db"


def _get_conn() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    os.makedirs(os.path.dirname(DB_PATH) or ".", exist_ok=True)
    with closing(_get_conn()) as conn:
        cursor = conn.cursor()
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS plate_records (
                id         INTEGER PRIMARY KEY AUTOINCREMENT,
                video_name TEXT,
                timestamp  TEXT NOT NULL,
                plate_no   TEXT NOT NULL,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
            """
        )
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS rush_events (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp    TEXT NOT NULL,
                plate_no     TEXT NOT NULL,
                warning_type TEXT NOT NULL,
                message      TEXT NOT NULL,
                image_path   TEXT,
                video_name   TEXT,
                created_at   TEXT DEFAULT CURRENT_TIMESTAMP
            )
            """
        )
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_plate_timestamp ON plate_records(timestamp)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_plate_no       ON plate_records(plate_no)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_rush_timestamp  ON rush_events(timestamp)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_rush_plate_no   ON rush_events(plate_no)")
        # 建唯一索引前先对存量数据去重（保留 id 最小的那一条）
        cursor.execute(
            "DELETE FROM plate_records WHERE id NOT IN ("
            "  SELECT MIN(id) FROM plate_records"
            "  GROUP BY video_name, timestamp, plate_no"
            ")"
        )
        cursor.execute(
            "DELETE FROM rush_events WHERE id NOT IN ("
            "  SELECT MIN(id) FROM rush_events"
            "  GROUP BY timestamp, plate_no, warning_type"
            ")"
        )
        # 唯一索引防止后续重复写入
        cursor.execute(
            "CREATE UNIQUE INDEX IF NOT EXISTS uq_plate_records "
            "ON plate_records(video_name, timestamp, plate_no)"
        )
        cursor.execute(
            "CREATE UNIQUE INDEX IF NOT EXISTS uq_rush_events "
            "ON rush_events(timestamp, plate_no, warning_type)"
        )
        conn.commit()


# ── 车牌记录 ──────────────────────────────────────────────────────────────────

def insert_plate_record(video_name: str, timestamp: str, plate_no: str) -> None:
    """实时写入单条过车记录，重复则忽略。"""
    with closing(_get_conn()) as conn:
        conn.execute(
            "INSERT OR IGNORE INTO plate_records (video_name, timestamp, plate_no) VALUES (?, ?, ?)",
            (os.path.basename(video_name), timestamp, plate_no),
        )
        conn.commit()


def insert_plate_records(video_name: str, records: Sequence[Tuple[str, str]]) -> None:
    """批量写入过车记录（视频结束兜底 / 迁移脚本），重复则忽略。"""
    if not records:
        return
    rows = [(os.path.basename(video_name), ts, plate_no) for ts, plate_no in records]
    with closing(_get_conn()) as conn:
        conn.executemany(
            "INSERT OR IGNORE INTO plate_records (video_name, timestamp, plate_no) VALUES (?, ?, ?)",
            rows,
        )
        conn.commit()


def update_plate_record(old_plate: str, new_plate: str, timestamp: str, video_name: str) -> int:
    """更新指定记录的车牌号，返回受影响行数。"""
    with closing(_get_conn()) as conn:
        cursor = conn.execute(
            "UPDATE plate_records SET plate_no = ? "
            "WHERE video_name = ? AND timestamp = ? AND plate_no = ?",
            (new_plate, os.path.basename(video_name), timestamp, old_plate),
        )
        conn.commit()
        return cursor.rowcount


def fetch_plate_records(
    limit: int = 1000,
    plate_no: Optional[str] = None,
    video_name: Optional[str] = None,
) -> List[Dict[str, str]]:
    """查询过车记录，支持按车牌/视频名模糊过滤。"""
    conditions: List[str] = []
    params: List = []
    if plate_no:
        conditions.append("plate_no LIKE ?")
        params.append(f"%{plate_no}%")
    if video_name:
        conditions.append("video_name LIKE ?")
        params.append(f"%{video_name}%")
    where = ("WHERE " + " AND ".join(conditions)) if conditions else ""
    params.append(limit)
    with closing(_get_conn()) as conn:
        rows = conn.execute(
            f"SELECT video_name, timestamp, plate_no "
            f"FROM plate_records {where} ORDER BY id DESC LIMIT ?",
            params,
        ).fetchall()
    return [
        {"video_name": r["video_name"], "timestamp": r["timestamp"], "plate_no": r["plate_no"]}
        for r in rows
    ]


# ── 冲岗事件 ──────────────────────────────────────────────────────────────────

def insert_rush_event(event: Dict[str, str], video_name: str = "") -> None:
    with closing(_get_conn()) as conn:
        conn.execute(
            "INSERT OR IGNORE INTO rush_events "
            "(timestamp, plate_no, warning_type, message, image_path, video_name) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (
                event.get("timestamp", ""),
                event.get("plate", ""),
                event.get("warning_type", ""),
                event.get("message", ""),
                event.get("image_path", ""),
                os.path.basename(video_name),
            ),
        )
        conn.commit()


def update_rush_plate(old_plate: str, new_plate: str) -> int:
    """将 rush_events 中所有旧车牌替换为新车牌，返回受影响行数。"""
    with closing(_get_conn()) as conn:
        cursor = conn.execute(
            "UPDATE rush_events SET plate_no = ? WHERE plate_no = ?",
            (new_plate, old_plate),
        )
        conn.commit()
        return cursor.rowcount


def fetch_rush_events(
    limit: int = 500,
    plate_no: Optional[str] = None,
    warning_type: Optional[str] = None,
) -> List[Dict[str, str]]:
    conditions: List[str] = []
    params: List = []
    if plate_no:
        conditions.append("plate_no LIKE ?")
        params.append(f"%{plate_no}%")
    if warning_type:
        conditions.append("warning_type = ?")
        params.append(warning_type)
    where = ("WHERE " + " AND ".join(conditions)) if conditions else ""
    params.append(limit)
    with closing(_get_conn()) as conn:
        rows = conn.execute(
            f"SELECT timestamp, plate_no, warning_type, message, image_path, video_name "
            f"FROM rush_events {where} ORDER BY id DESC LIMIT ?",
            params,
        ).fetchall()
    return [
        {
            "timestamp":    r["timestamp"],
            "plate":        r["plate_no"],
            "warning_type": r["warning_type"],
            "message":      r["message"],
            "image_path":   r["image_path"],
            "video_name":   r["video_name"],
        }
        for r in rows
    ]
