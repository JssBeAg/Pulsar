// logger.h — Local log on the internal flash (LittleFS), file /log.csv.
// Works without a network: it is the astronaut's "black box".
#pragma once
#include "readings.h"

// Mounts LittleFS (formats it if it cannot be mounted). Returns true when ready.
bool initLogger();

// Adds a line to the RAM buffer (does NOT write to flash).
// `ts` = seconds since the ESP32 booted (it has no real clock).
void addLine(const Readings& r, unsigned long ts);

// Writes the RAM buffer to /log.csv and empties it.
// Call every 30 s, NOT on every reading: flash wears out with many small writes.
void flushToFlash();
