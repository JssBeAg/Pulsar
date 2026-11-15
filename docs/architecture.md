# Pulsar architecture

One-page summary. Full detail is in the [README](../README.md).

## Scenario

Pulsar monitors the health of astronauts working on the **Moon**. Every component is on the lunar surface: the astronauts, the EVA radio link and the base station. There is **no link to Earth** in this system.

Astronauts go outside the base on extravehicular activity (EVA). A short-range radio link (the **EVA link**) carries their data to a laptop inside the base (the **base station**). That link drops whenever an astronaut walks out of range, passes behind terrain such as craters or rocks, or is blocked by base structures. On the lunar surface this is normal, so the system is designed around it.

## Components

| Component | Folder | Runs on | Role |
|---|---|---|---|
| **Astronaut** | `astronaut/sensor/` | ESP32 worn by the astronaut, outside the base | Reads the sensors, evaluates the rules locally, shows status + action on the OLED, logs to flash (LittleFS). Publishes over MQTT when in coverage. Never depends on the network. |
| Astronaut hardware notes | `astronaut/hardware/` | — | Wiring and parts list for the sensor. |
| **EVA link** | `comms/access-point/` | ESP32 access point, network `EVA-LINK` | Simulates the short-range EVA radio link. Only offers WiFi; runs no services. |
| **Base station** | `station/` | Laptop inside the lunar base | One shared program for every astronaut and every base computer. |
| Broker | `station/broker/` | Base station (Docker) | Mosquitto: receives the data from all astronauts. |
| Backend | `station/backend/` | Base station | Ingestor: subscribes to MQTT, stores data in SQLite, applies the rules, detects lost links. |
| Dashboard | `station/dashboard/` | Base station | Streamlit: only reads SQLite. Never connects to MQTT. |
| Database | `data/` | Base station | `pulsar.db` (SQLite), created at runtime. |

### Sensor firmware modules

| File | Responsibility |
|---|---|
| `readings.*` | Read the variables. The **only** file to change when using real sensors. |
| `rules.*` | Number → traffic light + reason + action. |
| `display.*` | OLED: overall status, action and pulse chart. |
| `logger.*` | CSV on LittleFS, written in one block every 30 s. |
| `network.*` | Non-blocking WiFi + MQTT (retries every 5 s). |
| `main.cpp` | Orchestrates everything with `millis()`, no `delay`. |

## Principles

- **Autonomy first.** The astronaut always has local alerts, the OLED and the log, whatever the state of the link.
- **Coverage loss is normal.** On the lunar surface the link drops with distance and terrain. The system treats this as an expected condition, not a failure.
- **One shared station.** One broker, one ingestor and one dashboard serve all astronauts and all base computers. There is no separate program per astronaut or per user.
- **Dashboard reads, never subscribes.** Only the backend talks to MQTT.
- **Lost-link detection lives in the station.** When the station gets no data from an astronaut for a configurable time (default 5 s), it marks that astronaut's link as lost. The EVA link cannot report its own outage, because it goes silent.

## Data flow

```
astronaut/sensor (outside, ESP32)
  potentiometers → readings → rules → OLED (status + action)
                        ↓
                  log in flash (LittleFS)
                        ↓  EVA-LINK, when in coverage
comms/access-point (EVA-LINK, ESP32, WiFi only)
                        ↓
station/ (lunar base laptop)
  Mosquitto → ingestor → SQLite → Streamlit dashboard
                                        ↑
            astronauts and base computers view it in a browser
```

1. Every 1 s the sensor reads, evaluates and shows the result on the OLED, and adds a line to its RAM log. Every 30 s it flushes the log to `/log.csv`.
2. When in coverage, it publishes `pulsar/<astronaut_id>/vitals` and `pulsar/<astronaut_id>/environment` (JSON without `ts`). Out of coverage it keeps working and skips publishing.
3. The ingestor (subscribed to `pulsar/+/vitals` and `pulsar/+/environment`, QoS 1) takes the astronaut ID from the topic, adds `ts`, saves to `readings`, evaluates and saves to `evaluations`.
4. The dashboard reads SQLite every 2 s for the astronaut selected in its sidebar.

## Topology

```
 [astronaut 1 ESP32] ─┐
 [astronaut 2 ESP32] ─┼── WiFi "EVA-LINK" ── [EVA link ESP32 AP, 192.168.4.1]
 [base computers   ] ─┘                                  │
                                                         │
                         [base station laptop, 192.168.4.10]
                           Mosquitto :1883  ·  Streamlit :8501
```

- One WiFi network (`EVA-LINK`), one subnet (`192.168.4.0/24`).
- The base station has a static IP, `192.168.4.10`, so the sensors know where the broker is.
- Each sensor has its own `ASTRONAUT_ID`, used in its MQTT topics and client ID.
- Base computers open the dashboard at `http://192.168.4.10:8501`.

## Lost-link rule

- **Rule:** if the station receives no message from an astronaut for `LINK_TIMEOUT` (default **5 s**, configurable), that astronaut's link is **lost**. It returns to **ok** as soon as a new message arrives.
- **Where:** in the station, because silence can only be detected by the receiver. The EVA link goes quiet when coverage drops and cannot announce it.
- **Meaning:** a lost link is about coverage, not about the astronaut's health. The astronaut still has their local alerts. The dashboard shows the last known status together with how long ago it arrived.

> **Current implementation:** the dashboard shows a "possible loss of communication" warning after 10 s without new data. Moving the detection into the ingestor with a 5 s default is pending.

## Decisions and why

| Decision | Why |
|---|---|
| **The sensor decides on its own (edge computing)** | The EVA link drops often. The astronaut must see their status and actions without the base. |
| **Lost-link detection in the station** | A silent access point cannot report itself; only the receiver notices that data stopped. |
| **Store on internal flash (LittleFS), not an SD card** | No SD reader needed. ~1.4 MB gives about 13 hours at 1 reading per second. |
| **CSV on the sensor, not SQLite** | Simple, robust, opens in Excel. SQLite on an ESP32 is heavy and fragile. |
| **Write to flash every 30 s, not on every reading** | Flash wears out with many small writes. |
| **A separate ESP32 as the EVA link** | Models the radio link as its own piece. Turning it off is a clear way to show coverage loss. |
| **Broker and services on the base station, not the access point** | The link only carries data; services belong on the station. |
| **One shared station program** | One broker, one ingestor, one dashboard: no duplicated data, one place to look. |
| **Dashboard reads SQLite, never MQTT** | One writer (the ingestor). The dashboard can be developed without hardware, and many browsers add no MQTT load. |
| **Python + Streamlit** | One language for the backend and dashboard; a working dashboard in hours. |
| **Non-blocking reconnection on the sensor** | Waiting for the network would freeze the OLED exactly when coverage drops. |
| **Astronaut ID in the topic** | One wildcard subscription (`pulsar/+/...`) serves any number of astronauts. |
