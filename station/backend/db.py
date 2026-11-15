"""Access to the Pulsar SQLite database.

Tables (README, section 9):
- readings:    ts, astronaut, variable, value   (one row per variable)
- evaluations: ts, astronaut, system, level, reason, action
- checkins:    ts, astronaut, mood, sleep, stress, note

Only the ingestor writes readings and evaluations; the dashboard only reads
(and writes check-ins).
"""

import sqlite3
from contextlib import closing

import config


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
