// config.h — Station network settings for the EVA-LINK access point.
//
// EVA-LINK joins the station network (a router or phone hotspot that the
// laptop with the broker is also on) as a WiFi client, and forwards the
// sensors' traffic to it with NAT.
#pragma once

// ===== Station network (STA side) =====
// TODO: set manually before flashing
constexpr const char* STATION_WIFI_SSID = "TODO_STATION_SSID";          // Station network name
// TODO: set manually before flashing
constexpr const char* STATION_WIFI_PASSWORD = "TODO_STATION_PASSWORD";  // Station network password

constexpr unsigned long STATION_RETRY_MS = 5000;  // Time between station connection attempts (ms)
