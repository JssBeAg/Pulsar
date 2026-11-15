"""Pulsar dashboard (Streamlit): HELLO/BYE state and EVA-LINK lost alert.

Reads SQLite only, never MQTT: the ingestor is the single subscriber, so
any number of browsers can open this page without touching the broker.

Run from the repo root:

    streamlit run station/dashboard/app.py
"""

import sys
import time
from datetime import datetime
from pathlib import Path

import pandas as pd
import streamlit as st

# Reuse the read helpers from station/backend/db.py (no MQTT code in there).
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "backend"))
import db  # noqa: E402

LOST_AFTER_S = 5          # Fallback: link is LOST when the last message is older than this (s)
REFRESH_S = 1             # Page refresh period (s)
HISTORY_ROWS = 20         # Rows in the recent-events table
SIM_PREFIX = "sim-"       # IDs used by fake_sensor.py

st.set_page_config(page_title="Pulsar", page_icon="🛰️", layout="wide")


def link_state(latest_screen: str, age_s: float) -> str:
    """UP or LOST. Either condition below is enough for LOST.

    1. The latest row is OFFLINE: the broker published the sensor's Last Will
       because its connection dropped. This is the reliable signal.
    2. The latest row is older than LOST_AFTER_S: a fallback for the case
       where the will is late or never arrives (it is only a guess).
    """
    if latest_screen == "OFFLINE" or age_s > LOST_AFTER_S:
        return "LOST"
    return "UP"


def source_of(astronaut_id: str) -> str:
    return "simulated" if astronaut_id.startswith(SIM_PREFIX) else "sensor"


def show_astronaut(con, astronaut_id: str, now: float) -> None:
    latest = db.latest_status(con, astronaut_id)
    if latest is None:
        return
    age_s = now - latest["received_at"]
    state = link_state(latest["screen"], age_s)

    if state == "LOST":
        st.error(f"EVA-LINK lost: {astronaut_id}")

    name, screen, age, link = st.columns(4)
    name.metric("Astronaut", astronaut_id)
    if state == "LOST":
        # Not a live value: the last screen the sensor itself sent, in grey.
        known = db.last_known_screen(con, astronaut_id) or "none"
        screen.markdown(f"Screen\n\n:gray[last known: {known}]")
    else:
        screen.metric("Screen", latest["screen"])
    age.metric("Since last message", f"{age_s:.0f} s")
    link.metric("Link", state)


def show_history(con, astronaut_id: str) -> None:
    rows = db.recent_status(con, astronaut_id, HISTORY_ROWS)
    if not rows:
        return
    table = pd.DataFrame(rows)
    table["received_at"] = table["received_at"].map(lambda t: datetime.fromtimestamp(t).strftime("%H:%M:%S"))
    table["source"] = source_of(astronaut_id)
    st.dataframe(table[["id", "received_at", "source", "screen", "uptime_s"]], hide_index=True)


def render() -> None:
    st.title("🛰️ Pulsar: EVA-LINK status")

    con = db.connect_reader()
    if con is None:
        st.info("No database yet. Start the ingestor: `python station/backend/ingestor.py`")
        return
    try:
        astronauts = db.list_astronauts(con) if _has_status_table(con) else []
        if not astronauts:
            st.info("No status events yet. Is a sensor (or `fake_sensor.py`) publishing?")
            return

        now = time.time()
        for astronaut_id in astronauts:
            show_astronaut(con, astronaut_id, now)

        st.divider()
        selected = st.selectbox("Recent events for", astronauts, key="history_astronaut")
        st.subheader(f"Last {HISTORY_ROWS} events: {selected}")
        show_history(con, selected)
    finally:
        con.close()


def _has_status_table(con) -> bool:
    row = con.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name='status_events'").fetchone()
    return row is not None


# Refresh every second. st.fragment reruns only this part; older Streamlit
# versions without it fall back to reloading the whole page.
if hasattr(st, "fragment"):
    st.fragment(run_every=REFRESH_S)(render)()
else:
    render()
    time.sleep(REFRESH_S)
    st.rerun()
