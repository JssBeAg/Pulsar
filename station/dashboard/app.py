"""Pulsar dashboard (Streamlit).

One shared program for the astronauts inside the habitat and the ground team:
several browsers can open it at the same time.

Reads SQLite, NEVER connects to MQTT: that way it can be developed without the
ESP32, and opening several screens does not duplicate data.

Run from the repo root:

    streamlit run station/dashboard/app.py --server.address 0.0.0.0
"""

import sqlite3
import sys
import time
from contextlib import closing
from datetime import datetime
from pathlib import Path

import pandas as pd
import streamlit as st

# Reuse config.py and db.py from the backend.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "backend"))
import config  # noqa: E402
import db  # noqa: E402

REFRESH_INTERVAL_S = 2
CHART_WINDOW_S = 60 * 60      # Last hour
NO_DATA_WARNING_S = 10        # Warn if no data arrives within this time

LEVEL_EMOJI = {"GREEN": "🟢", "YELLOW": "🟡", "RED": "🔴"}
SEVERITY = {"GREEN": 0, "YELLOW": 1, "RED": 2}
SYSTEM_ORDER = ["Cardiovascular", "Respiratory", "Thermal", "Environment"]

st.set_page_config(page_title="Pulsar", page_icon="🛰️", layout="wide")
db.create_tables()  # In case the ingestor has not run yet


def query(sql, params=()):
    """Runs a read-only query and returns a DataFrame."""
    with closing(sqlite3.connect(config.DB_PATH)) as con:
        return pd.read_sql_query(sql, con, params=params)


def known_astronauts():
    """Every astronaut ID that has sent data or recorded a check-in."""
    df = query(
        """SELECT astronaut FROM readings
           UNION
           SELECT astronaut FROM checkins
           ORDER BY astronaut"""
    )
    return df["astronaut"].tolist()


def latest_evaluations(astronaut):
    """Latest evaluation of each system for one astronaut."""
    return query(
        """SELECT ts, system, level, reason, action FROM evaluations
           WHERE rowid IN (
               SELECT MAX(rowid) FROM evaluations WHERE astronaut = ? GROUP BY system
           )""",
        (astronaut,),
    )


def recent_readings(astronaut, variables, since_ts):
    placeholders = ",".join("?" for _ in variables)
    return query(
        f"""SELECT ts, variable, value FROM readings
            WHERE astronaut = ? AND variable IN ({placeholders}) AND ts >= ?
            ORDER BY ts""",
        (astronaut, *variables, since_ts),
    )


def live_section(astronaut):
    """Traffic light per system, recommended action and charts. Refreshes itself."""
    st.title(f"🛰️ Pulsar: {astronaut}")

    evaluations = latest_evaluations(astronaut)
    if evaluations.empty:
        st.info(f"No evaluations for {astronaut} yet.")
        return

    # Warn if data stopped arriving (for example, the comms link was unplugged).
    seconds_without_data = int(time.time()) - int(evaluations["ts"].max())
    if seconds_without_data > NO_DATA_WARNING_S:
        st.warning(f"No new data for {seconds_without_data} s. Possible loss of communication.")

    # Overall status = the most severe system
    evaluations["severity"] = evaluations["level"].map(SEVERITY)
    worst = evaluations.sort_values("severity", ascending=False).iloc[0]
    st.header(f"{LEVEL_EMOJI.get(worst['level'], '⚪')} Overall status: {worst['level']}")
    st.subheader(f"Recommended action: {worst['action']}")
    st.caption(f"{worst['system']}: {worst['reason']}")

    # Traffic light per system
    columns = st.columns(len(SYSTEM_ORDER))
    for column, system in zip(columns, SYSTEM_ORDER):
        row = evaluations[evaluations["system"] == system]
        with column:
            if row.empty:
                st.markdown(f"### ⚪ {system}")
                st.write("No data")
                continue
            row = row.iloc[0]
            st.markdown(f"### {LEVEL_EMOJI.get(row['level'], '⚪')} {system}")
            st.write(row["reason"])
            st.caption(row["action"])

    # Charts for the last hour
    readings = recent_readings(astronaut, ["pulse", "spo2"], int(time.time()) - CHART_WINDOW_S)
    if readings.empty:
        return
    readings["time"] = pd.to_datetime(readings["ts"].map(datetime.fromtimestamp))
    table = readings.pivot_table(index="time", columns="variable", values="value")

    left, right = st.columns(2)
    with left:
        st.markdown("**Pulse (bpm), last hour**")
        if "pulse" in table:
            st.line_chart(table["pulse"])
    with right:
        st.markdown("**SpO₂ (%), last hour**")
        if "spo2" in table:
            st.line_chart(table["spo2"])


def checkin_section(astronaut):
    """Daily behavioral-health check-in for the selected astronaut."""
    st.divider()
    st.header(f"🧠 Daily check-in: {astronaut}")
    # The form key includes the astronaut so switching astronauts starts a fresh form.
    with st.form(f"checkin-{astronaut}", clear_on_submit=True):
        mood = st.slider("Mood (1 = very bad, 5 = very good)", 1, 5, 3)
        sleep = st.slider("Sleep (1 = very bad, 5 = very good)", 1, 5, 3)
        stress = st.slider("Stress (1 = none, 5 = a lot)", 1, 5, 3)
        note = st.text_area("Note (optional)")
        if st.form_submit_button("Save check-in"):
            db.save_checkin(int(time.time()), astronaut, mood, sleep, stress, note.strip())
            st.success("Check-in saved.")
    # TODO: recommendations from the check-ins (RF8), e.g. 3 days of high stress and poor sleep.


# ===== Astronaut selector (sidebar) =====
astronauts = known_astronauts()
st.sidebar.title("Pulsar")
if not astronauts:
    st.title("🛰️ Pulsar")
    st.info("No astronauts yet. Is `python station/backend/ingestor.py` running and a sensor publishing?")
    st.stop()
selected = st.sidebar.selectbox("Astronaut", astronauts)
st.sidebar.caption("New astronauts appear here when the page reloads.")

# Auto-refresh every 2 s.
# With st.fragment only the live part is redrawn, so the check-in form is not
# interrupted while someone fills it in. If the Streamlit version is old and
# has no st.fragment, the whole page reloads with sleep + rerun.
if hasattr(st, "fragment"):
    st.fragment(run_every=REFRESH_INTERVAL_S)(live_section)(selected)
    checkin_section(selected)
else:
    live_section(selected)
    checkin_section(selected)
    time.sleep(REFRESH_INTERVAL_S)
    st.rerun()
