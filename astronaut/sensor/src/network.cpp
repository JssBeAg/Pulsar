// network.cpp — Non-blocking WiFi + MQTT implementation.
// Data contract: README, section 9. If you change it, tell the team.
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "network.h"
#include "config.h"

static const unsigned long RETRY_INTERVAL_MS = 5000;  // How often to retry connecting

// Built from ASTRONAUT_ID (config.h) by joining string literals at compile time.
// The client ID must be unique per device, or the broker kicks out the older connection.
static const char* MQTT_CLIENT_ID = "pulsar-" ASTRONAUT_ID "-sensor";
static const char* TOPIC_VITALS = "pulsar/" ASTRONAUT_ID "/vitals";
static const char* TOPIC_ENVIRONMENT = "pulsar/" ASTRONAUT_ID "/environment";

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static unsigned long lastWifiAttempt = 0;
static unsigned long lastMqttAttempt = 0;

void initNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // Starts the attempt and returns immediately
  lastWifiAttempt = millis();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
}

void maintainNetwork() {
  unsigned long now = millis();

  // 1) No WiFi: retry every 5 s and return (no point trying MQTT without WiFi).
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiAttempt >= RETRY_INTERVAL_MS) {
      lastWifiAttempt = now;
      Serial.println("[network] No WiFi, retrying...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  }

  // 2) WiFi but no MQTT: retry every 5 s.
  if (!mqttClient.connected()) {
    if (now - lastMqttAttempt >= RETRY_INTERVAL_MS) {
      lastMqttAttempt = now;
      Serial.println("[network] Connecting to MQTT...");
      // TODO: connect() opens a TCP connection and can take a few seconds if the
      // laptop does not answer (WiFiClient timeout). Measure it on the hardware;
      // if it freezes the OLED, lower the client timeout.
      if (mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.println("[network] MQTT connected.");
      }
    }
    return;
  }

  // 3) Everything connected: service the MQTT client (keeps the connection alive).
  mqttClient.loop();
}

bool mqttConnected() {
  return mqttClient.connected();
}

void publishVitals(const Readings& r) {
  if (!mqttConnected()) return;  // No network: do nothing

  // Note: PubSubClient can only publish with QoS 0. The ingestor subscribes with
  // QoS 1, but the delivered QoS is the lower of the two, so this hop is QoS 0.
  char json[96];
  snprintf(json, sizeof(json), "{\"pulse\":%.0f,\"spo2\":%.0f,\"temp\":%.1f}",
           r.pulse, r.spo2, r.temp);
  mqttClient.publish(TOPIC_VITALS, json);

  snprintf(json, sizeof(json), "{\"co2\":%.0f,\"rad\":%.2f}", r.co2, r.rad);
  mqttClient.publish(TOPIC_ENVIRONMENT, json);
}
