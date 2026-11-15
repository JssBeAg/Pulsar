# Pulsar 🛰️

**Health monitoring for astronauts on the Moon that keeps working outside the base, even when the EVA radio link to the base station drops.**

Project for the **NASA Space Apps Challenge 2026** (November 14, 2026).

---

## Contents sisi

1. [The problem](#1-the-problem)
2. [Our solution](#2-our-solution)
3. [Architecture](#3-architecture)
4. [Why we designed it this way](#4-why-we-designed-it-this-way)
5. [Tools](#5-tools)
6. [Requirements](#6-requirements)
7. [Repository structure](#7-repository-structure)
8. [How to run it](#8-how-to-run-it)
9. [Data contract (MQTT)](#9-data-contract-mqtt)
10. [Health rules](#10-health-rules)
11. [The demo](#11-the-demo)
12. [Status and priorities](#12-status-and-priorities)
13. [How we work with Git](#13-how-we-work-with-git)
14. [Glossary](#14-glossary)

---

## 1. The problem

On a long mission on the Moon the astronaut's body goes through constant changes in many places: the heart, the bones, the immune system, sleep and mood. On top of that, the main risks are:

- **Radiation**
- **Isolation and confinement**
- **Altered gravity**
- **A closed, hostile environment** (for example, high CO₂)

The problem is that **there is no doctor nearby**. Outside the base, on extravehicular activity (EVA), the astronaut depends on a short-range radio link to the base, and that link drops whenever they walk out of range, pass behind a crater or rock, or are blocked by base structures. The astronaut needs something that tells them **"you're fine"** or **"something is wrong, do this"**, without depending on anyone else.

The challenge asks for a system that:
- collects health indicators,
- lets the astronaut **assess and act** on their condition (not just look at charts),
- works **offline and locally**,
- includes **behavioral health** (mind, not just body).

## 2. Our solution

Everything runs on the Moon; there is no link to Earth. Pulsar has three parts:

1. **The astronaut's device (ESP32), worn outside the base.** It measures their vital signs, decides whether they are fine or not, shows it on a display with a **traffic light** (green, yellow or red) and a **recommended action**, and keeps a log. **It works on its own, with no network.** When it is in coverage, it also sends its data to the base.
2. **The EVA link (a second ESP32 acting as the `EVA-LINK` access point).** It simulates the short-range extravehicular radio link that carries the data to the base. It only offers WiFi. Its coverage drops with distance, terrain and base structures, and that is expected.
3. **The base station (laptop inside the lunar base).** It receives the data, stores the history, analyzes multi-day trends, records behavioral health and notices when an astronaut's link is lost. Every astronaut and every computer in the base uses **this same program** from a browser, and it handles several astronauts at once.

> **Core idea:** the astronaut's device **does not depend** on the link. Losing coverage is normal on the lunar surface; the base station is an extra, not a dependency.

For now the vital signs are **simulated with potentiometers** (knobs): turning them lets us fake a pulse of 40 or 170 whenever we want for the demo. Later they get swapped for real sensors without touching the rest of the system.

## 3. Architecture

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

Each sensor publishes under its own ID (`pulsar/<astronaut_id>/...`), so several astronauts share the same EVA link and base station. Full detail, including the topology and the lost-link rule, is in [`docs/architecture.md`](docs/architecture.md).

**Principles:**
- **Autonomy first:** the astronaut always has local alerts, the OLED and the log, whatever the link does.
- **Coverage loss is normal:** the EVA link drops with distance and terrain; that is an expected condition, not a failure.
- **One shared station:** one broker, one ingestor and one dashboard serve every astronaut and every base computer.
- **Dashboard reads, never subscribes:** only the backend talks to MQTT.
- **Lost-link detection lives in the station:** no data from an astronaut for a configurable time (default 5 s) means their link is lost. The EVA link cannot report its own outage, because it goes silent.

### Who does what

| Piece | Folder | In real life it would be… | Its job |
|---|---|---|---|
| **Astronaut sensor** (ESP32) | `astronaut/sensor/` | A wearable health monitor on the EVA suit | Measure, decide and alert the astronaut. **Always works.** Publishes over MQTT when in coverage. |
| **EVA link** (ESP32 access point) | `comms/access-point/` | The short-range EVA radio link | **Only offer WiFi** (`EVA-LINK`). Its coverage comes and goes. |
| **Mosquitto** | `station/broker/` | The base's message "mail carrier" | Receive the data from all astronauts and deliver it to subscribers. |
| **ingestor.py** | `station/backend/` | A base station service | Subscribe to MQTT, store the data, apply the rules and detect lost links. |
| **SQLite** | `data/` | The base's mission database | Store the history. |
| **Streamlit** | `station/dashboard/` | Monitoring screens in the base | Read SQLite and show traffic lights, charts, actions and the mood check-in. |

### The system told in versions

Each piece exists because it solves a problem left by the previous one:

1. **Measure:** a potentiometer + ESP32 gives us a number. *Problem: nobody sees it.*
2. **Show and decide on the device:** OLED display + rules. The astronaut now knows whether they are fine. *Problem: there is no history and nobody else sees it.*
3. **Send:** over WiFi to the laptop. *Problem: in what format? And what if several programs want the data?*
4. **A mail carrier (MQTT):** the ESP32 **publishes** to a "mailbox" and any **subscribed** program gets a copy. *Problem: messages get lost.*
5. **Store:** a Python program saves everything to SQLite. *Problem: a number alone does not say whether it is fine.*
6. **Rules:** the number becomes green, yellow or red + an action.
7. **Personal baseline:** the system learns each astronaut's "normal" and detects changes that would be normal in someone else.
8. **Dashboard:** Streamlit shows everything on the base computers.
9. **Behavioral health:** a daily check-in for mood, sleep and stress.

## 4. Why we designed it this way

| Decision | Why |
|---|---|
| **The ESP32 decides on its own (edge computing)** | The EVA link drops often outside the base. When it does, the astronaut still sees their status and the action to take. |
| **Lost-link detection in the base station** | The EVA link goes silent when coverage drops, so it cannot report the outage. Only the receiver can notice that data stopped. |
| **Store on the ESP32's internal memory (LittleFS), not an SD card** | We have no SD reader. The internal flash has ~1.4 MB free: about 13 hours at 1 reading per second, or ~5 days at 1 every 10 seconds. Enough for the demo with no extra hardware. |
| **CSV on the ESP32, not SQLite** | SQLite on an ESP32 is possible but heavy and easy to break. A CSV is simple and opens in Excel. |
| **Write to flash every 30–60 s, not on every reading** | Flash memory wears out with many small writes. We collect lines in RAM and write them all at once. |
| **A second ESP32 as the EVA link** | It models the radio link as its own piece, and the event WiFi is bad. Turning it off in front of the judges is a very visual way to show coverage loss. |
| **Mosquitto on the laptop, not on the access point** | The EVA link should only carry data; the services belong on the base station. On a real mission this would live on dedicated, redundant hardware (for example, a Raspberry Pi as a *gateway*). Today the laptop is both the server and the screen. |
| **Streamlit reads SQLite, not MQTT** | Only one program (`ingestor.py`) writes to the database. That way the dashboard can be developed without the ESP32, and opening several screens does not duplicate data. |
| **Python + Streamlit (not Next.js)** | Everything in one language and a dashboard in hours, not all day. Several PCs can view it from a browser. Next.js is left as an improvement after the hackathon. |
| **WiFi reconnection does not block the program** | If the ESP32 sits "waiting" for coverage, the display freezes exactly when the link drops. It retries every few seconds without stopping anything else. |
| **One shared base station program** | Every astronaut and every base computer uses the same broker, ingestor and dashboard: no duplicated data and one place to look. |

## 5. Tools

### Hardware
- 2 × **ESP32** (classic DevKit): one sensor and one access point
- Linear **B10K potentiometers**: they simulate the sensors
- **SSD1306 OLED display** 128×64 (I2C)
- Breadboard, jumper wires and a USB **data** cable
- Laptop (Windows)

Wiring and parts list: [`astronaut/hardware/README.md`](astronaut/hardware/README.md).

### Software
| Tool | What for |
|---|---|
| **VS Code** | Editor for the whole project |
| **PlatformIO** (VS Code extension) | Build and upload the firmware to the ESP32 |
| **Arduino framework** (C++) | Firmware language |
| **Docker / Docker Compose** | Run Mosquitto without installing it by hand |
| **Mosquitto** (`eclipse-mosquitto:2`) | MQTT broker |
| **Python 3** | Backend and dashboard |
| **paho-mqtt** | Lets Python connect to MQTT |
| **SQLite** | Database (included with Python) |
| **Streamlit** | Web dashboard |
| **pandas** | Handle the data for the charts |
| **MQTT Explorer** | Watch MQTT messages live (debugging) |
| **Git + GitHub** | Version control |

### Firmware libraries (PlatformIO)
- `knolleary/PubSubClient`: MQTT client
- `adafruit/Adafruit SSD1306` and `adafruit/Adafruit GFX Library`: OLED display
- `LittleFS` and `Preferences`: included in the ESP32 core

## 6. Requirements

### Functional requirements
- **RF1.** The device reads pulse, SpO₂, temperature, CO₂ and radiation (simulated).
- **RF2.** The device classifies each system as green, yellow or red and shows a recommended action on the display.
- **RF3.** The device learns a personal baseline and warns when a value drifts from what is normal *for that person*.
- **RF4.** The device keeps a local log even with no network.
- **RF5.** When there is a network, the device sends the data over MQTT to the station.
- **RF6.** The station stores the history in a database.
- **RF7.** The dashboard shows the status of each system, charts and recommended actions.
- **RF8.** The astronaut can record a daily check-in for mood, sleep and stress, and the system generates recommendations from it.
- **RF9.** Several computers can view the dashboard at the same time.

### Non-functional requirements
- **RNF1. Autonomy:** the device works without WiFi, without the laptop and without internet.
- **RNF2. Offline:** no part of the system needs internet.
- **RNF3. Non-blocking:** losing the network never freezes the display or stops the log.
- **RNF4. Replaceable:** swapping a potentiometer for a real sensor only changes the reading function.
- **RNF5. Simple:** it can be brought up on a laptop with Docker and Python.

### Out of scope (for now)
- Real sensors (MAX30102, MPU6050, MLX90614/DS18B20, MQ-135/SCD40)
- SD card
- More than one physical sensor device (the software already supports several astronauts; each device needs its own `ASTRONAUT_ID`)
- Authentication and TLS on the broker (the demo uses anonymous access)
- Automatically re-sending what was stored while there was no network (done if time allows)

## 7. Repository structure

The repo is organized by **where each component physically runs**.

```
pulsar/
├── astronaut/                   # Worn by the astronaut. Works with no network.
│   ├── sensor/                  # ESP32 firmware (PlatformIO project)
│   │   ├── platformio.ini
│   │   └── src/
│   │       ├── main.cpp         # Orchestrates everything with millis(), no delay
│   │       ├── config.h         # ASTRONAUT_ID, WiFi, MQTT, pins and intervals
│   │       ├── readings.*       # Read variables (the only file to change for real sensors)
│   │       ├── rules.*          # Number → traffic light + reason + action
│   │       ├── display.*        # OLED: status, action and pulse chart
│   │       ├── logger.*         # CSV on LittleFS, written every 30 s
│   │       └── network.*        # Non-blocking WiFi + MQTT
│   └── hardware/
│       └── README.md            # Wiring and parts list
├── comms/                       # The EVA link
│   └── access-point/            # ESP32 firmware (PlatformIO project), EVA-LINK WiFi only
├── station/                     # The base station laptop: one program for all astronauts and base computers
│   ├── broker/
│   │   └── mosquitto.conf
│   ├── backend/
│   │   ├── config.py            # Broker, topics, database path
│   │   ├── db.py                # Tables and save functions
│   │   ├── rules.py             # Same rules as the firmware, in Python
│   │   └── ingestor.py          # MQTT → SQLite (the only writer of readings)
│   └── dashboard/
│       └── app.py               # Streamlit: reads SQLite, traffic lights, charts and check-in
├── data/                        # pulsar.db is created here at runtime (not committed)
├── docs/
│   ├── architecture.md          # Architecture and decisions summary
│   └── rules.md                 # Thresholds: source of truth
├── docker-compose.yml           # Runs Mosquitto
├── requirements.txt
├── .gitignore
└── README.md
```

`.gitignore`:
```
.venv/
__pycache__/
.pio/
*.db
```

## 8. How to run it

> The base code for every part already exists, but **it has not been tested on the hardware yet**. This section gets updated as each part starts working.
>
> Every command runs from the **repo root**.

### 8.1 Set up Python (with internet, before joining the access point)
```powershell
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```
`requirements.txt`:
```
paho-mqtt
streamlit
pandas
```

### 8.2 Start the broker
`station/broker/mosquitto.conf`:
```
listener 1883 0.0.0.0
allow_anonymous true
```
`docker-compose.yml`:
```yaml
services:
  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
    volumes:
      - ./station/broker/mosquitto.conf:/mosquitto/config/mosquitto.conf
```
```powershell
docker compose up -d
```
Check it with **MQTT Explorer** (host `localhost`, port `1883`).

### 8.3 EVA link (ESP32 access point)
1. Open `comms/access-point/` as its own PlatformIO project and upload the firmware. It creates the `EVA-LINK` network (password `pulsar2026`).
2. Connect the laptop to that WiFi network.
3. Give the laptop a **static IP**: `192.168.4.10` (mask `255.255.255.0`, gateway `192.168.4.1`).
4. In **Windows Firewall**, open TCP ports **1883** (MQTT) and **8501** (Streamlit) for the **Public** profile.

> ⚠️ While connected to the access point, the laptop **has no internet**. To push to GitHub, switch networks or use a cable.

### 8.4 ESP32 sensor
Open `astronaut/sensor/` as its own PlatformIO project (separate from `comms/access-point/`).

Before uploading, set a **unique** `ASTRONAUT_ID` in `astronaut/sensor/src/config.h` (default `"astro1"`). It goes into every MQTT topic and into the MQTT client ID; two devices with the same ID would mix their data and kick each other off the broker.

`astronaut/sensor/platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
lib_deps =
    knolleary/PubSubClient
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
```
Wiring (more detail in [`astronaut/hardware/README.md`](astronaut/hardware/README.md)):

| Component | ESP32 pin |
|---|---|
| Pulse potentiometer (middle leg) | GPIO 34 |
| SpO₂ potentiometer (middle leg) | GPIO 35 |
| Temperature potentiometer (middle leg) | GPIO 32 |
| CO₂ potentiometer (middle leg) | GPIO 33 |
| Radiation potentiometer (middle leg) | GPIO 36 |
| Potentiometers (outer legs) | 3.3V and GND |
| OLED SDA / SCL | GPIO 21 / GPIO 22 |
| OLED VCC / GND | 3.3V / GND |

> We use only ADC1 pins because **ADC2 does not work while WiFi is on**. GPIO 39 is free for another sensor.

The astronaut ID, pins, WiFi network, broker IP and intervals are changed in `astronaut/sensor/src/config.h`.

Upload the firmware and open the serial monitor (115200). Every second it prints the readings, the system shown on the display and whether MQTT is connected (`mqtt=yes/no`).

The display shows:
- at the top, in large text, the **overall status** (the worst level across all systems);
- below it, the **recommended action**;
- at the bottom, the current pulse and a **chart of the last 100 seconds**.

The local log is stored in `/log.csv` on the flash (columns `ts,pulse,spo2,temp,co2,rad`, where `ts` = seconds since the ESP32 booted). It is written every 30 s; once it passes ~1 MB it is deleted and starts again so the memory does not fill up.

### 8.5 Backend and dashboard
```powershell
python station/backend/ingestor.py
streamlit run station/dashboard/app.py --server.address 0.0.0.0
```
- The ingestor creates `data/pulsar.db` the first time and retries on its own if the broker is not up yet.
- On the laptop: `http://localhost:8501`
- From another PC on the same network: `http://192.168.4.10:8501`
- Pick the astronaut in the **sidebar**. The status, the one-hour charts and the check-in form all apply to that astronaut. New astronauts appear in the list when the page reloads.
- The dashboard refreshes every 2 s. If no data arrives for 10 s, it shows a possible-loss-of-communication warning.

## 9. Data contract (MQTT)

This is the agreed "language" between the firmware and the backend. **If you change it, tell the team.**

| Topic | Who publishes | Content |
|---|---|---|
| `pulsar/<astronaut_id>/vitals` | ESP32 sensor | `{"pulse": 72, "spo2": 98, "temp": 36.6}` |
| `pulsar/<astronaut_id>/environment` | ESP32 sensor | `{"co2": 850, "rad": 0.8}` |
| `pulsar/<astronaut_id>/status` | Station (optional) | Computed status |

- `<astronaut_id>` is the sensor's `ASTRONAUT_ID`, for example `pulsar/astro1/vitals`.
- 1 message per second per astronaut.
- The payload has no `ts`: the ingestor sets the time when it receives the message, so the ESP32 does not need a clock.
- The ingestor subscribes to `pulsar/+/vitals` and `pulsar/+/environment` with **QoS 1** and reads the astronaut ID from the topic. The sensor's MQTT library (PubSubClient) only publishes with QoS 0, so in practice the sensor → broker hop is QoS 0.

### Database (SQLite)
| Table | Columns |
|---|---|
| `readings` | ts, astronaut, variable, value |
| `evaluations` | ts, astronaut, system, level, reason, action |
| `checkins` | ts, astronaut, mood, sleep, stress, note |

`readings` uses one row per variable: adding a new sensor does not require changing the table. The `astronaut` column comes from the topic, so every table holds every astronaut.

## 10. Health rules

> ⚠️ **Demo** values, not medical criteria.
>
> The **source of truth** is [`docs/rules.md`](docs/rules.md) (it includes how values exactly on a boundary are handled and the text of each action). If you change a threshold, also change it in `astronaut/sensor/src/rules.cpp` and `station/backend/rules.py`.

| System | Variable | Green | Yellow | Red |
|---|---|---|---|---|
| Cardiovascular | Pulse (bpm) | 50–100 | 40–50 / 100–130 | <40 / >130 |
| Respiratory | SpO₂ (%) | ≥95 | 90–94 | <90 |
| Thermal | Temperature (°C) | 36–37.5 | 35.5–36 / 37.5–38.5 | <35.5 / >38.5 |
| Environment | CO₂ (ppm) | <1000 | 1000–2500 | >2500 |
| Radiation | Accumulated dose | Compared against a daily limit | | |

Range each potentiometer simulates: pulse 40–180 bpm, SpO₂ 85–100 %, temperature 35–40 °C, CO₂ 400–5000 ppm, simulated radiation.

**Each rule returns three things:** level, reason and action. Example:
> 🟡 *SpO₂ 91 %, below your normal (97 %).* → Check the sensor, reduce activity and verify the cabin's life support.

**Personal baseline:** during the first few minutes the system computes each astronaut's average and variation. If a value drifts far from *their* normal (more than 2 deviations), it turns yellow even if it is within the general range. It is stored in NVS memory so it is not lost if the ESP32 restarts.

**Behavioral health:** daily check-in (mood, sleep and stress, from 1 to 5). Example rule:
> 3 days in a row with high stress and poor sleep → contact the mission psychologist in the next communication window + decompression routine.

## 11. The demo

1. Everything running: the OLED and the dashboard show the same data.
2. We turn a knob → the pulse goes up → the OLED and the dashboard turn red with their action.
3. **The EVA link drops** ("the astronaut walked behind a crater"): we take the `EVA-LINK` access point down.
4. The OLED **keeps working** and the log keeps being saved. The astronaut is never left without information, and the dashboard shows that the link was lost.
5. Coverage returns → the base station starts receiving data again.

### Next steps (for the pitch)
- On a real mission the broker would live on **dedicated, redundant hardware**, not a laptop.
- Broker **authentication and TLS**, which the demo leaves out.
- Real sensors, SD card, multiple astronauts and a more polished interface.

## 12. Status and priorities

The base code for items 1 to 6 is already written; they get checked off once they work on the hardware. Known pending items: radiation rule (the daily limit is not defined yet), personal baseline and recommendations from the check-in.

In order:

- [ ] **1.** ESP32: readings + rules + OLED (the autonomous core)
- [ ] **2.** ESP32 access point
- [ ] **3.** MQTT sending → Mosquitto → `ingestor.py` → SQLite
- [ ] **4.** Streamlit dashboard (traffic lights + actions + charts)
- [ ] **5.** Behavioral-health check-in
- [ ] **6.** CSV log on LittleFS
- [ ] **7.** Personal baseline
- [ ] **8.** *(If time allows)* Re-send stored data on reconnect
- [ ] Final README, video and presentation

### Work split by area
| Area | Branch | What it includes |
|---|---|---|
| Firmware | `firmware` | ESP32 sensor (`astronaut/`) and access point (`comms/`) |
| Backend | `backend` | Ingestor, rules and SQLite (`station/backend/`) |
| Dashboard | `dashboard` | Streamlit and check-in (`station/dashboard/`) |
| Docs / pitch | `docs` | README, diagrams, video and presentation |

## 13. How we work with Git

- One branch per area (see the table above). `main` must always work.
- Before starting: **pull**. When something works: **commit + push**.
- Small commits with clear messages, for example `firmware: pulse reading and traffic light on OLED`.
- If you change the **MQTT contract** or the **tables**, tell the team: it breaks everyone else.

## 14. Glossary

| Term | What it means |
|---|---|
| **ESP32** | A cheap microcontroller with WiFi. It is the device's "brain". |
| **Potentiometer** | A knob that changes a voltage. The ESP32 reads it and we treat it as if it were a vital sign. |
| **ADC** | Analog-to-digital converter: turns a voltage (0–3.3V) into a number (0–4095). |
| **OLED** | The small display where the astronaut sees their status. |
| **I2C / SPI** | Ways to connect components to the ESP32 with few wires. |
| **Access point** | A device that creates a WiFi network for others to join. |
| **EVA** | Extravehicular activity: work done outside the base, on the lunar surface. |
| **EVA link** | The short-range radio link between an astronaut outside and the base. Simulated by the `EVA-LINK` access point. |
| **Coverage** | The area where the EVA link reaches the astronaut. Distance, terrain and base structures limit it. |
| **Base station** | The laptop inside the lunar base that runs the broker, ingestor and dashboard. |
| **MQTT** | A lightweight messaging protocol widely used in IoT. It works with "mailboxes" (topics). |
| **Broker** | MQTT's "mail carrier": it receives messages and delivers them to subscribers. We use Mosquitto. |
| **Publish / subscribe** | Send a message to a topic / ask to receive a topic's messages. |
| **Topic** | The name of the "mailbox", for example `pulsar/astro1/vitals`. A `+` in a subscription matches any one level, so `pulsar/+/vitals` covers every astronaut. |
| **QoS** | MQTT "quality of service": 0 = at most once, 1 = at least once. |
| **Edge computing** | Processing data on the device itself, not on a distant server. |
| **Store-and-forward** | Storing data when there is no network and sending it when it comes back. |
| **LittleFS** | A file system that uses the ESP32's internal memory like a small disk. |
| **NVS / Preferences** | ESP32 memory for storing small amounts of data that survive a restart. |
| **Baseline** | What is "normal" for one specific person. |
| **SQLite** | A database that is just a file; it needs no server. |
| **Streamlit** | A Python library for quickly building web pages with charts. |
| **Docker** | A tool for running programs in "containers" without installing them directly. |

---

Made by the Pulsar team for the NASA Space Apps Challenge 2026.
