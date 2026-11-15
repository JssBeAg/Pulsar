// config.h — Settings for the station network access point (PULSAR-BASE).
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// ===== Network =====
constexpr const char* NETWORK_SSID = "PULSAR-BASE";            // Name the station computers and EVA-LINK join
// Demo only, not a mission value: a real base would use per-device credentials.
constexpr const char* NETWORK_PASSWORD = "pulsar-base-2026";   // WPA2 password (needs 8+ characters)
// 2.4 GHz only, the ESP32 limit: the radio has no 5 GHz band.
constexpr int NETWORK_CHANNEL = 6;                             // WiFi channel (1–13)

// The subnet is 10.10.0.0/24 because EVA-LINK uses 192.168.4.0/24. EVA-LINK
// sits on both networks at once; overlapping subnets would leave it unable
// to tell which interface a packet belongs to, and routing would break.
const IPAddress NETWORK_IP(10, 10, 0, 1);                      // This access point's address
const IPAddress NETWORK_GATEWAY(10, 10, 0, 1);                 // The AP is its own gateway (there is no upstream)
const IPAddress NETWORK_SUBNET(255, 255, 255, 0);              // /24 network

// One slot for EVA-LINK, three for station computers. Each client costs the
// ESP32 RAM and airtime, so the limit is kept small on purpose.
constexpr int MAX_CLIENTS = 4;                                 // Max simultaneous WiFi clients (count)

// ===== Status output =====
constexpr unsigned long STATUS_INTERVAL_MS = 5000;             // How often the client list is printed (ms)
