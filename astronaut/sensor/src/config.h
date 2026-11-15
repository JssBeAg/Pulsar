// config.h — Configuration for the astronaut sensor (button test step).
// In this step the push button is the only data source: it temporarily
// stands in for the potentiometers, which are not read yet. The current
// screen is published to the station's MQTT broker.
#pragma once

// ===== EVA link (WiFi access point in comms/access-point) =====
constexpr const char* WIFI_SSID = "EVA-LINK";         // Network name of the EVA link
constexpr const char* WIFI_PASSWORD = "pulsar2026";   // WPA2 password of the EVA link

// ===== Station (MQTT broker on the laptop) =====
// The laptop is on the station network, behind EVA-LINK's NAT, so this is
// its IP on the station network, not a 192.168.4.x address.
// TODO: set manually before flashing
constexpr const char* STATION_HOST = "10.44.241.206";  // Laptop's IP on the station network
constexpr int MQTT_PORT = 1883;                       // Mosquitto port on the laptop
constexpr const char* ASTRONAUT_ID = "astro1";        // Unique ID of this astronaut; topic pulsar/<id>/status
constexpr int MQTT_KEEPALIVE_S = 5;                   // Broker publishes our Last Will after ~1.5x this of silence (s)
constexpr unsigned long HEARTBEAT_MS = 2000;          // Publish the current screen at least this often (ms)
constexpr unsigned long MQTT_RETRY_MS = 5000;         // Time between MQTT connection attempts (ms)
constexpr unsigned long MQTT_CONNECT_TIMEOUT_MS = 1000; // Max wait for the TCP connect to the broker (ms)
constexpr int MQTT_SOCKET_TIMEOUT_S = 1;              // Max wait for the broker's CONNACK (s)
// Status is published at QoS 0: PubSubClient only publishes QoS 0.

// ===== Station acknowledgments (link dot) =====
constexpr const char* ACK_TOPIC_FORMAT = "pulsar/%s/ack"; // Ack topic, printf format; %s = ASTRONAUT_ID
// Two heartbeats: one missed ack keeps the dot on, two turn it off.
// Matched to the dashboard, which marks the link LOST after LOST_AFTER_S = 5 s
// without a message: the dot goes out at 4 s, just before the dashboard does.
constexpr unsigned long ACK_TIMEOUT_MS = 4000;        // Dot off when no ack for this long (ms)
constexpr unsigned long INDICATOR_REFRESH_MS = 200;   // Check the link dot at most this often (ms)

// ===== Button =====
constexpr int BUTTON_PIN = 13;                        // Push button to GND (GPIO), pressed = LOW
constexpr unsigned long DEBOUNCE_MS = 50;             // Ignore the pin this long after an accepted change (ms)

// ===== Log =====
constexpr const char* LOG_PATH = "/log.csv";          // CSV log of button presses on LittleFS (path)
constexpr const char* PUBLISH_LOG_PATH = "/publish.csv"; // CSV log of publish attempts on LittleFS (path)

// ===== Timing =====
constexpr unsigned long WIFI_RETRY_MS = 5000;         // Time between WiFi connection attempts (ms)
constexpr unsigned long STATUS_INTERVAL_MS = 5000;    // How often the status line is printed (ms)
