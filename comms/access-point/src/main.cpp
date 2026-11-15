// main.cpp — ESP32 access point: the comms link.
// It simulates the real link (radio or satellite) that carries the astronaut's
// data to the station. It only offers WiFi and does nothing else.
// Unplugging it simulates losing communication.
#include <Arduino.h>
#include <WiFi.h>

static const char* NETWORK_NAME = "PULSAR-COMMS";
static const char* NETWORK_PASSWORD = "pulsar2026";

// Access point IP. The station laptop uses the static IP 192.168.4.10 on this network.
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress GATEWAY(192, 168, 4, 1);
static const IPAddress SUBNET_MASK(255, 255, 255, 0);

static const unsigned long STATUS_INTERVAL_MS = 5000;  // How often the status is printed
static unsigned long lastStatus = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, GATEWAY, SUBNET_MASK);
  bool created = WiFi.softAP(NETWORK_NAME, NETWORK_PASSWORD);

  if (created) {
    Serial.printf("[comms] Network \"%s\" created. IP: %s\n", NETWORK_NAME, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[comms] ERROR: could not create the network.");
  }
}

void loop() {
  // Prints how many devices are connected, without using delay.
  unsigned long now = millis();
  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = now;
    Serial.printf("[comms] IP %s | connected devices: %d\n",
                  WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum());
  }
}
