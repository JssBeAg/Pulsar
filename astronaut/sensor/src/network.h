// network.h — Non-blocking WiFi connection to EVA-LINK. MQTT lives in mqtt.h.
// The network is optional: nothing here ever makes the screen or the log wait.
#pragma once

// Name clash workaround: the ESP32 Arduino core 3.x has its own "Network.h",
// which WiFi.h includes. Windows file names ignore case and src/ is searched
// first, so the core lands on THIS file instead of its own. Pulling in the
// core's header here, with include_next (continue the search after src/),
// gives the core its definitions back. On a case-sensitive system the core
// finds its own Network.h directly and this line is harmless.
#include_next <Network.h>

// Starts WiFi in station mode and begins connecting. Returns at once.
void startNetwork();

// Call on every loop pass. Retries every WIFI_RETRY_MS while disconnected and
// prints each change: connected (IP, RSSI) or lost.
void maintainNetwork();

// true while connected to EVA-LINK.
bool networkUp();
