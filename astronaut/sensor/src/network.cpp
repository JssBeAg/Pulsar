// network.cpp — Non-blocking WiFi connection to EVA-LINK.
#include <Arduino.h>
#include <WiFi.h>
#include "network.h"
#include "config.h"

static unsigned long lastAttemptAt = 0;  // When the last connection attempt started (millis)
static bool wasConnected = false;        // Connection state at the last check

void startNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // Starts the attempt and returns immediately
  lastAttemptAt = millis();
  Serial.println("Connecting to EVA-LINK...");
}

// The connection is retried on a timer instead of waiting in a loop. Coverage
// on the lunar surface comes and goes, and the screen and the log must keep
// working the whole time, so this function always returns immediately.
void maintainNetwork() {
  unsigned long now = millis();
  bool connected = networkUp();

  if (connected && !wasConnected) {
    Serial.printf("Connected to EVA-LINK, IP %s, RSSI %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else if (!connected && wasConnected) {
    Serial.println("EVA-LINK lost");
    lastAttemptAt = now;  // Wait one interval before the first retry
  }
  wasConnected = connected;

  if (!connected && now - lastAttemptAt >= WIFI_RETRY_MS) {
    lastAttemptAt = now;
    Serial.println("Connecting to EVA-LINK...");
    WiFi.disconnect();                     // Abandon any half-finished attempt
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

bool networkUp() {
  return WiFi.status() == WL_CONNECTED;
}
