# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**基于机器视觉的高速收费站卡口冲岗行为识别系统** — A desktop application that analyzes toll-gate surveillance video to detect and record vehicles running red lights ("冲岗"). It performs real-time license plate recognition, toll-bar state detection, and alerts on two types of violations: tailgating (跟车冲岗) and bar-ramming (撞杆冲岗).

## Running the Application

```bash
python main.py
```

Requires a CUDA-capable GPU. The application loads `best.pt` (YOLO model) onto CUDA at startup.

**Required assets** (must exist alongside `main.py`):
- `best.pt` — YOLOv8 model for toll-bar state detection (classes: 0=抬起, 1=关闭, 2=损坏)
- `simhei.ttf` — Font for rendering Chinese text onto video frames
- `alert.wav` — Audio file for violation alerts
- `background.png` — UI background image
- `config.json` — Runtime configuration (auto-created with defaults if missing)

## Historical Data Migration

To migrate legacy flat-file data into SQLite:
```bash
python migrate_to_db.py
```
This is idempotent — it checks for duplicates before inserting.

## Architecture

### Module Responsibilities

| File | Role |
|---|---|
| `main.py` | PyQt5 GUI, video processing loop, detection logic, event recording |
| `db.py` | SQLite abstraction — schema init, insert/query functions |
| `migrate_to_db.py` | One-time migration from `plate_records.txt` + `rush_records.json` to SQLite |

### Data Flow

```
Video file (mp4/avi/mov)
  → QTimer (30ms) → update_frame()
      → YOLO (every 10 frames) → toll-bar state → rush detection logic
      → HyperLPR3 (every 7 frames) → plate recognition
          → Levenshtein dedup (distance < 3 = same plate, discard)
  → On video end: save_plates_to_file() → plate_records.txt + plate_records DB table
  → On rush event: record_rush_event() → screenshot JPG + rush_records.json + rush_events DB table
```

### Rush Detection Logic (`check_for_rush` / `handle_rod_state_no_change`)

- **跟车冲岗**: More than one distinct plate detected while the bar is raised in a single open cycle (`current_up_plates > 1`).
- **撞杆冲岗**: Bar enters "损坏" state while a vehicle is present.
- Flags `self.a` and `self.b` prevent duplicate event recording within one incident.

### Dual Persistence (legacy compatibility)

Rush events are written to **both** `rush_events/rush_records.json` (legacy) and the SQLite DB. When loading the event list, the app reads from the DB first and only falls back to the JSON file if the DB returns no results.

### Database Schema (`db.py`)

```
plate_records: id, video_name, timestamp (TEXT), plate_no, created_at
rush_events:   id, timestamp (TEXT), plate_no, warning_type, message, image_path, video_name, created_at
```
Indexes on `timestamp` and `plate_no` for both tables.

### Configuration (`config.json`)

```json
{
  "model_path": "best.pt",
  "font_path": "simhei.ttf",
  "confidence_threshold": 0.975,
  "alert_sound": "alert.wav"
}
```
All paths are relative to the working directory. The `confidence_threshold` filters HyperLPR3 results — only plates with confidence ≥ this value are accepted.

## Known Issues

- **Connection leak**: `_get_conn()` returns connections that are never explicitly closed (`with conn:` in sqlite3 only commits/rolls back, does not close).
- **No UNIQUE constraint** on `plate_records`: deduplication in `migrate_to_db.py` is done in Python memory, which is inefficient for large datasets.
- **timestamp stored as TEXT**: Sorting and range queries depend on consistent `YYYY-MM-DD HH:MM:SS` format from all sources.
