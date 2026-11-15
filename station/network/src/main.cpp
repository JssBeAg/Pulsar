// main.cpp — ESP32 access point: the station network (station/network).
//
// Simulates the lunar base's local network. The station computers (laptops
// running the broker, the ingestor or just a browser for the dashboard) and
// the EVA-LINK ESP32 join it as WiFi clients. Its only job is to let those
// clients reach each other on 10.10.0.0/24.
//
// There is deliberately no internet, no NAT and no upstream connection: the
// base network is closed, and everything the mission needs lives on it.
//
// Everything is driven by millis(); there is no delay() anywhere.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_ap_get_sta_list.h>
#include "config.h"

// ===== State =====
static int lastClientCount = 0;          // Client count at the last check, to detect joins and leaves
static unsigned long lastStatusAt = 0;   // When the client list was last printed (millis)

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

void setupNetwork() {
  WiFi.mode(WIFI_AP);
  // Addressing first, so the DHCP server hands out 10.10.0.x from the very
  // first client instead of the core's default 192.168.4.x, which would
  // collide with EVA-LINK.
  if (!WiFi.softAPConfig(NETWORK_IP, NETWORK_GATEWAY, NETWORK_SUBNET)) {
    Serial.println("[network] ERROR: softAPConfig failed");
  }
  if (!WiFi.softAP(NETWORK_SSID, NETWORK_PASSWORD, NETWORK_CHANNEL, 0, MAX_CLIENTS)) {
    Serial.println("[network] ERROR: softAP failed, the network is NOT up");
    return;
  }
  // The IP is read back from the interface, not from config.h, so a
  // mismatch between what was asked and what is running is visible here.
  Serial.printf("%s ready, IP %s, channel %d, max clients %d\n",
                NETWORK_SSID, WiFi.softAPIP().toString().c_str(), NETWORK_CHANNEL, MAX_CLIENTS);
}

// ---------------------------------------------------------------------------
// Clients
// ---------------------------------------------------------------------------

// Prints one line per join or leave. It compares the client count with the
// last value instead of using WiFi event callbacks: the count is a plain
// query that works the same in every core version.
void checkClientChanges() {
  int count = WiFi.softAPgetStationNum();
  if (count == lastClientCount) return;

  if (count > lastClientCount) {
    Serial.printf("client connected (%d/%d)\n", count, MAX_CLIENTS);
  } else {
    Serial.printf("client disconnected (%d/%d)\n", count, MAX_CLIENTS);
  }
  lastClientCount = count;
}

// Prints the client count and, for each client, its MAC and IP.
// The WiFi driver knows the MACs (association); the IPs come from the DHCP
// server's leases, matched to the MACs by esp_wifi_ap_get_sta_list_with_ip.
// A client that has associated but not finished DHCP shows "no IP yet".
void printStatus() {
  unsigned long now = millis();
  if (now - lastStatusAt < STATUS_INTERVAL_MS) return;
  lastStatusAt = now;

  wifi_sta_list_t stations;
  if (esp_wifi_ap_get_sta_list(&stations) != ESP_OK) {
    Serial.printf("[status] clients=%d (could not read the client list)\n", WiFi.softAPgetStationNum());
    return;
  }
  Serial.printf("[status] clients=%d/%d\n", stations.num, MAX_CLIENTS);

  wifi_sta_mac_ip_list_t withIp;
  bool haveIps = esp_wifi_ap_get_sta_list_with_ip(&stations, &withIp) == ESP_OK;

  for (int i = 0; i < stations.num; i++) {
    const uint8_t* mac = stations.sta[i].mac;
    Serial.printf("  %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (!haveIps) {
      Serial.println("  IP unavailable");
    } else if (withIp.sta[i].ip.addr == 0) {
      Serial.println("  no IP yet");
    } else {
      // esp_ip4_addr_t holds the address in network byte order, which is the
      // byte order IPAddress(uint32_t) expects on the ESP32.
      Serial.printf("  %s\n", IPAddress(withIp.sta[i].ip.addr).toString().c_str());
    }
  }
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  setupNetwork();
}

void loop() {
  checkClientChanges();
  printStatus();
}
