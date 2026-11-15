// mqtt.cpp — MQTT publisher and ack listener (PubSubClient).
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>
#include "mqtt.h"
#include "config.h"
#include "logger.h"

static const char* OFFLINE_PAYLOAD = "{\"screen\": \"OFFLINE\"}";

static WiFiClient netClient;
static PubSubClient mqttClient(netClient);

static char topic[64];                   // pulsar/<ASTRONAUT_ID>/status
static char ackTopic[64];                // pulsar/<ASTRONAUT_ID>/ack
static char clientId[48];                // pulsar-sensor-<ASTRONAUT_ID>
static bool hostConfigured = false;      // false while STATION_HOST is still a placeholder
static bool wasConnected = false;        // MQTT state at the last check
static bool wasNetworkUp = false;        // EVA-LINK state at the last check
static unsigned long lastAttemptAt = 0;  // When the last connection attempt started (millis)
static unsigned long lastPublishAt = 0;  // When the screen was last published (millis)
static bool ackSeen = false;             // At least one ack arrived on the current connection
static unsigned long lastAckAt = 0;      // When the last ack arrived (millis)

// Called from mqttClient.loop() for every incoming message. Arriving on the
// ack topic is all the dot needs, so the payload is not parsed.
static void onMessage(char* msgTopic, byte* payload, unsigned int length) {
  if (strcmp(msgTopic, ackTopic) != 0) return;
  lastAckAt = millis();
  ackSeen = true;
}

void startMqtt() {
  snprintf(topic, sizeof(topic), "pulsar/%s/status", ASTRONAUT_ID);
  snprintf(ackTopic, sizeof(ackTopic), ACK_TOPIC_FORMAT, ASTRONAUT_ID);
  snprintf(clientId, sizeof(clientId), "pulsar-sensor-%s", ASTRONAUT_ID);

  hostConfigured = strncmp(STATION_HOST, "TODO", 4) != 0;
  if (!hostConfigured) {
    Serial.println("[mqtt] STATION_HOST is not set in config.h. MQTT disabled.");
    return;
  }

  mqttClient.setServer(STATION_HOST, MQTT_PORT);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE_S);
  mqttClient.setCallback(onMessage);
  // Short timeouts: PubSubClient's connect() blocks until the TCP connect
  // and the CONNACK finish, so these bound how long one attempt can hold
  // the loop when the broker is unreachable.
  mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  netClient.setConnectionTimeout(MQTT_CONNECT_TIMEOUT_MS);
  Serial.printf("[mqtt] broker %s:%d, topic %s, acks on %s\n", STATION_HOST, MQTT_PORT, topic, ackTopic);
}

bool mqttUp() {
  return hostConfigured && mqttClient.connected();
}

bool mqttLive() {
  return mqttUp() && ackSeen && millis() - lastAckAt < ACK_TIMEOUT_MS;
}

// One attempt. The Last Will is registered with the broker as part of the
// CONNECT packet; the broker publishes it if this connection ends without a
// clean DISCONNECT (EVA-LINK shadow, power loss, keepalive timeout).
static void tryConnect() {
  Serial.printf("[mqtt] connecting to %s:%d...\n", STATION_HOST, MQTT_PORT);
  bool ok = mqttClient.connect(clientId, nullptr, nullptr,
                               topic, 1, false, OFFLINE_PAYLOAD, true);
  if (!ok) {
    Serial.printf("[mqtt] connect failed, state %d\n", mqttClient.state());
    return;
  }
  // Clean session: the broker forgets subscriptions on disconnect, so the
  // ack topic is subscribed again on every connect.
  ackSeen = false;  // An ack from the previous connection does not count
  if (!mqttClient.subscribe(ackTopic, 0)) {
    Serial.printf("[mqtt] ERROR: could not subscribe to %s\n", ackTopic);
  }
}

// Builds the JSON by hand, publishes it and logs the attempt.
static void publish(const char* screen) {
  unsigned long now = millis();
  lastPublishAt = now;

  bool published = false;
  if (mqttUp()) {
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"screen\": \"%s\", \"uptime_s\": %lu}", screen, now / 1000);
    published = mqttClient.publish(topic, payload);  // QoS 0: the only level PubSubClient publishes
  }
  logPublish(now, screen, published);
}

void publishScreen(const char* screen) {
  publish(screen);
  Serial.printf("[mqtt] button: %s %s\n", screen, mqttUp() ? "sent" : "NOT sent");
}

void maintainMqtt(bool networkUp, const char* screen) {
  unsigned long now = millis();

  if (hostConfigured) {
    if (!networkUp && wasNetworkUp) {
      // EVA-LINK is gone, so the TCP socket is dead even if it does not know
      // it yet. Drop it locally (no DISCONNECT packet: it could not arrive,
      // and the broker must publish our Last Will).
      netClient.stop();
      lastAttemptAt = now;
    }

    bool connected = mqttUp();
    if (connected != wasConnected) {
      Serial.println(connected ? "[mqtt] connected" : "[mqtt] connection lost");
      if (!connected) lastAttemptAt = now;  // Wait one interval before the first retry
    }
    wasConnected = connected;

    if (connected) {
      mqttClient.loop();  // Receives the acks, sends the keepalive PINGs, notices a dead connection
    } else if (networkUp && now - lastAttemptAt >= MQTT_RETRY_MS) {
      lastAttemptAt = now;
      tryConnect();
      if (mqttUp()) {
        wasConnected = true;
        Serial.println("[mqtt] connected");
        publish(screen);  // Report the current screen right away; its ack lights the dot
      }
    }
  }
  wasNetworkUp = networkUp;

  // The heartbeat runs even without a connection, so the publish log shows
  // the gaps (published = 0) too.
  if (now - lastPublishAt >= HEARTBEAT_MS) {
    publish(screen);
  }
}
