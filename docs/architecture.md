# Pulsar architecture

One-page summary. Full detail is in the [README](../README.md).

## Core idea

The astronaut's device **works on its own**. The network and the laptop are an extra, not a dependency.

The repo is organized by **where each component physically runs**:

| Folder | Runs on | Contents |
|---|---|---|
| `astronaut/` | The device the astronaut wears outside the habitat | ESP32 sensor firmware and its hardware notes |
| `comms/` | The link between astronaut and station | ESP32 access point firmware (simulates the radio or satellite link; WiFi only) |
| `station/` | The laptop inside the habitat | Mosquitto config, ingestor, database code and dashboard |

The station is **one shared program**: the astronauts inside the habitat and the ground team open the same dashboard, and several browsers and MQTT clients can connect to it at once. It handles **several astronauts**: each sensor publishes under its own `ASTRONAUT_ID`.

```
               ┌──────────── ESP32 sensor (worn by the astronaut) ───────────┐
potentiometers→│ read → rules + baseline → OLED display (value + traffic light)│
               │                 ↓                                           │
               │        CSV log on the internal flash                        │
               └─────────────────┬───────────────────────────────────────────┘
                                 │ WiFi (only when available)
                     [ESP32 #2 = access point, "the comms link"]
                                 │
  Station laptop (inside the habitat):
     Mosquitto (Docker) → ingestor.py → SQLite → Streamlit (dashboard)
                                                      ↑
                     astronauts in the habitat and the ground team, from a browser
```

## Who does what

| Piece | Code | Its job |
|---|---|---|
| ESP32 sensor | `astronaut/sensor/` | Measure, decide and alert the astronaut. Always works. |
| ESP32 access point | `comms/access-point/` | Only offer WiFi. Unplugging it simulates losing communication. |
| Mosquitto | `station/broker/`, `docker-compose.yml` | Receive MQTT messages and deliver them to subscribers. Anonymous and unencrypted for the demo only. |
| Ingestor | `station/backend/ingestor.py` | The only program that writes to SQLite: subscribes to `pulsar/+/...`, reads the astronaut ID from the topic, saves readings and evaluations. |
| Dashboard | `station/dashboard/app.py` | Reads SQLite. A sidebar selects the astronaut; shows traffic lights, actions, charts and the check-in. |

### Sensor firmware modules

| File | Responsibility |
|---|---|
| `readings.*` | Read the variables. The **only** file to change when using real sensors. |
| `rules.*` | Number → traffic light + reason + action. |
| `display.*` | OLED: overall status, action and pulse chart. |
| `logger.*` | CSV on LittleFS, written in one block every 30 s. |
| `network.*` | Non-blocking WiFi + MQTT (retries every 5 s). |
| `main.cpp` | Orchestrates everything with `millis()`, no `delay`. |

## Decisions and why

| Decision | Why |
|---|---|
| **The ESP32 decides on its own (edge computing)** | The challenge asks us not to depend on Earth. If the network goes down, the astronaut still sees their status. |
| **Store on the ESP32's internal memory (LittleFS), not an SD card** | We have no SD reader. The internal flash has ~1.4 MB free: about 13 hours at 1 reading per second, or ~5 days at 1 every 10 seconds. Enough for the demo with no extra hardware. |
| **CSV on the ESP32, not SQLite** | SQLite on an ESP32 is possible but heavy and easy to break. A CSV is simple and opens in Excel. |
| **Write to flash every 30–60 s, not on every reading** | Flash memory wears out with many small writes. We collect lines in RAM and write them all at once. |
| **A second ESP32 as the access point** | The event WiFi is bad. Also, unplugging it in front of the judges is a very visual way to simulate losing communication. |
| **Mosquitto on the laptop, not on the access point** | The comms link should only carry data; the services belong on the station. On a real mission this would live on dedicated, redundant hardware (for example, a Raspberry Pi as a *gateway*). Today the laptop is both the server and the screen. |
| **Streamlit reads SQLite, not MQTT** | Only one program (`ingestor.py`) writes to the database. That way the dashboard can be developed without the ESP32, and opening several screens does not duplicate data. |
| **Python + Streamlit (not Next.js)** | Everything in one language and a dashboard in hours, not all day. Several PCs can view it from a browser. Next.js is left as an improvement after the hackathon. |
| **WiFi reconnection does not block the program** | If the ESP32 sits "waiting" for the network, the display freezes and we lose exactly what we want to demonstrate. It retries every few seconds without stopping anything else. |

## Data flow

1. Every 1 s the sensor reads, evaluates and shows the result on the OLED, and adds a line to the RAM log.
2. When there is a network, it publishes `pulsar/<astronaut_id>/vitals` and `pulsar/<astronaut_id>/environment` (JSON without `ts`).
3. Every 30 s it flushes the log to `/log.csv`.
4. The ingestor (subscribed with QoS 1) receives each message, takes the astronaut ID from the topic, adds `ts`, saves to `readings`, evaluates and saves to `evaluations`.
5. The dashboard reads SQLite every 2 s for the astronaut selected in the sidebar.
