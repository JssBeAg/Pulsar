"""Access to the Pulsar SQLite database.

The database lives only on the base station, in data/pulsar.db.

Tables:
- status_events: id, received_at, astronaut_id, screen, uptime_s
  (button test step: HELLO/BYE state published by the sensor, or OFFLINE
  published by the broker as the sensor's Last Will when its link drops)
- readings:      ts, astronaut, variable, value   (vitals step, one row per variable)
- evaluations:   ts, astronaut, system, level, reason, action
- checkins:      ts, astronaut, mood, sleep, stress, note

Only the ingestor writes sensor data. The dashboard only reads. There is no
MQTT code here: this module is shared with the dashboard, which must stay
off the broker.
"""

import os
import sqlite3
from contextlib import closing
from pathlib import Path

import config

DEFAULT_DB = "data/pulsar.db"  # Relative to the repo root

# ---------------------------------------------------------------------------
# status_events (button test step)
# ---------------------------------------------------------------------------


def db_path() -> Path:
    """Path of the SQLite file: $PULSAR_DB (default data/pulsar.db), relative to the repo root.

    Resolving from the repo root, not the current directory, means the
    ingestor and the dashboard always open the same file wherever they start.
    """
    path = Path(os.environ.get("PULSAR_DB", DEFAULT_DB))
    return path if path.is_absolute() else config.REPO_ROOT / path


def connect_writer() -> sqlite3.Connection:
    """Opens the ingestor's connection, creating the file and the schema if needed."""
    path = db_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(path)
    # WAL lets the dashboard read while the ingestor writes, without
    # "database is locked" errors on either side.
    con.execute("PRAGMA journal_mode=WAL")
    create_status_schema(con)
    return con


def connect_reader() -> sqlite3.Connection | None:
    """Opens a read-only connection for the dashboard, or None if the file does not exist yet.

    mode=ro makes it impossible for the dashboard to write by mistake.
    """
    path = db_path()
    if not path.exists():
        return None
    return sqlite3.connect(f"{path.as_uri()}?mode=ro", uri=True)


def create_status_schema(con: sqlite3.Connection) -> None:
    """Creates the status_events table and its index if they do not exist."""
    con.execute(
        """CREATE TABLE IF NOT EXISTS status_events (
               id INTEGER PRIMARY KEY AUTOINCREMENT,
               received_at REAL NOT NULL,
               astronaut_id TEXT NOT NULL,
               screen TEXT NOT NULL,
               uptime_s INTEGER
           )"""
    )
    con.execute(
        "CREATE INDEX IF NOT EXISTS idx_status_astronaut_time "
        "ON status_events (astronaut_id, received_at)"
    )
    con.commit()


def insert_status(
    con: sqlite3.Connection, received_at: float, astronaut_id: str, screen: str, uptime_s: int | None
) -> None:
    """Inserts one status event and commits, so a crash never loses an accepted message."""
    con.execute(
        "INSERT INTO status_events (received_at, astronaut_id, screen, uptime_s) VALUES (?, ?, ?, ?)",
        (received_at, astronaut_id, screen, uptime_s),
    )
    con.commit()


def list_astronauts(con: sqlite3.Connection) -> list[str]:
    """Every astronaut ID that has sent at least one status event."""
    rows = con.execute("SELECT DISTINCT astronaut_id FROM status_events ORDER BY astronaut_id").fetchall()
    return [row[0] for row in rows]


def latest_status(con: sqlite3.Connection, astronaut_id: str) -> dict | None:
    """The most recent status event of one astronaut, or None."""
    row = con.execute(
        "SELECT received_at, screen, uptime_s FROM status_events "
        "WHERE astronaut_id = ? ORDER BY received_at DESC, id DESC LIMIT 1",
        (astronaut_id,),
    ).fetchone()
    if row is None:
        return None
    return {"received_at": row[0], "screen": row[1], "uptime_s": row[2]}


def last_known_screen(con: sqlite3.Connection, astronaut_id: str) -> str | None:
    """The most recent screen the sensor itself sent (HELLO or BYE), skipping OFFLINE rows, or None."""
    row = con.execute(
        "SELECT screen FROM status_events "
        "WHERE astronaut_id = ? AND screen != 'OFFLINE' ORDER BY received_at DESC, id DESC LIMIT 1",
        (astronaut_id,),
    ).fetchone()
    return row[0] if row else None


def recent_status(con: sqlite3.Connection, astronaut_id: str, limit: int = 20) -> list[dict]:
    """The last `limit` status events of one astronaut, newest first."""
    rows = con.execute(
        "SELECT id, received_at, screen, uptime_s FROM status_events "
        "WHERE astronaut_id = ? ORDER BY received_at DESC, id DESC LIMIT ?",
        (astronaut_id, limit),
    ).fetchall()
    return [{"id": r[0], "received_at": r[1], "screen": r[2], "uptime_s": r[3]} for r in rows]


# ---------------------------------------------------------------------------
# Vitals tables (earlier design, kept for the vitals step; not used by the
# status ingestor or the status dashboard)
# ---------------------------------------------------------------------------


def _connect():
    """Opens a new connection (creates the data/ folder if missing)."""
    config.DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    return sqlite3.connect(config.DB_PATH)


def create_tables():
    """Creates the tables if they do not exist yet."""
    # closing() closes the connection; "with con" commits (or rolls back on error).
    with closing(_connect()) as con, con:
        con.execute(
            """CREATE TABLE IF NOT EXISTS readings (
                   ts INTEGER NOT NULL,
                   astronaut TEXT NOT NULL,
                   variable TEXT NOT NULL,
                   value REAL NOT NULL
               )"""
        )
        con.execute(
            """CREATE TABLE IF NOT EXISTS evaluations (
                   ts INTEGER NOT NULL,
                   astronaut TEXT NOT NULL,
                   system TEXT NOT NULL,
                   level TEXT NOT NULL,
                   reason TEXT,
                   action TEXT
               )"""
        )
        con.execute(
            """CREATE TABLE IF NOT EXISTS checkins (
                   ts INTEGER NOT NULL,
                   astronaut TEXT NOT NULL,
                   mood INTEGER NOT NULL,
                   sleep INTEGER NOT NULL,
                   stress INTEGER NOT NULL,
                   note TEXT
               )"""
        )


def save_reading(ts, astronaut, variable, value):
    """Saves one variable's value (pulse, spo2, temp, co2, rad...)."""
    with closing(_connect()) as con, con:
        con.execute(
            "INSERT INTO readings (ts, astronaut, variable, value) VALUES (?, ?, ?, ?)",
            (ts, astronaut, variable, value),
        )


def save_evaluation(ts, astronaut, system, level, reason, action):
    """Saves the result of applying a rule (traffic light + reason + action)."""
    with closing(_connect()) as con, con:
        con.execute(
            "INSERT INTO evaluations (ts, astronaut, system, level, reason, action) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (ts, astronaut, system, level, reason, action),
        )


def save_checkin(ts, astronaut, mood, sleep, stress, note):
    """Saves the daily behavioral-health check-in (values from 1 to 5)."""
    with closing(_connect()) as con, con:
        con.execute(
            "INSERT INTO checkins (ts, astronaut, mood, sleep, stress, note) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (ts, astronaut, mood, sleep, stress, note),
        )
