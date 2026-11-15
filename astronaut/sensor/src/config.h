// config.h — Configuration constants for the ESP32 sensor.
// Everything that can be "tuned" without touching the logic lives here.
#pragma once

// ===== Astronaut identity =====
// Unique per device. It is part of every MQTT topic (pulsar/<ASTRONAUT_ID>/...)
// and of the MQTT client ID, so each astronaut needs a different value.
// Kept as a #define so network.cpp can build the topics at compile time.
#define ASTRONAUT_ID "astro1"

// ===== EVA link WiFi (created by the comms/access-point ESP32) =====
constexpr const char* WIFI_SSID = "EVA-LINK";    // Network name
constexpr const char* WIFI_PASS = "pulsar2026";  // Network password

// ===== MQTT broker (Mosquitto on the base station laptop) =====
constexpr const char* MQTT_HOST = "192.168.4.10";  // Base station's static IP on the EVA-LINK network
constexpr int MQTT_PORT = 1883;                    // Standard MQTT port

// ===== Potentiometer pins (ADC1 only: ADC2 does not work while WiFi is on) =====
constexpr int PIN_PULSE = 34;  // Simulates heart rate
constexpr int PIN_SPO2  = 35;  // Simulates oxygen saturation
constexpr int PIN_TEMP  = 32;  // Simulates body temperature
constexpr int PIN_CO2   = 33;  // Simulates cabin CO2
constexpr int PIN_RAD   = 36;  // Simulates radiation

// ===== OLED display (I2C) =====
constexpr int PIN_OLED_SDA = 21;  // I2C data line
constexpr int PIN_OLED_SCL = 22;  // I2C clock line

// ===== Timing =====
constexpr unsigned long SAMPLE_INTERVAL_MS = 1000;  // How often to read, evaluate, display and publish
constexpr unsigned long FLUSH_INTERVAL_MS = 30000;  // How often the log is written to flash
