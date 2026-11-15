// logger.cpp — CSV log implementation on LittleFS.
#include <Arduino.h>
#include <LittleFS.h>
#include "logger.h"

static const char* LOG_PATH = "/log.csv";
static const char* CSV_HEADER = "ts,pulse,spo2,temp,co2,rad\n";

// Maximum file size: ~1 MB.
// The esp32dev LittleFS partition is ~1.4 MB. If the file filled the
// partition, writes would start failing and we would stop saving exactly
// the most recent data (what matters most). LittleFS also needs free
// blocks to work well. So once the file passes ~1 MB it is deleted and
// started again: we would rather lose old data than stop logging.
static const size_t MAX_FILE_SIZE = 1024UL * 1024UL;

// RAM buffer limit. At 1 line per second with a flush every 30 s it uses
// ~1.5 KB; the limit only protects against the flash failing several times in a row.
static const size_t MAX_BUFFER_SIZE = 8 * 1024;

static String ramBuffer;
static bool loggerReady = false;

bool initLogger() {
  // begin(true) = format if it cannot be mounted (for example, the first time).
  loggerReady = LittleFS.begin(true);
  ramBuffer.reserve(2048);
  if (!loggerReady) {
    Serial.println("[logger] Could not mount LittleFS. No log will be saved.");
  }
  return loggerReady;
}

void addLine(const Readings& r, unsigned long ts) {
  if (ramBuffer.length() >= MAX_BUFFER_SIZE) {
    return;  // Buffer full (flash is not accepting data): the line is dropped
  }
  char line[96];
  snprintf(line, sizeof(line), "%lu,%.0f,%.0f,%.2f,%.0f,%.2f\n",
           ts, r.pulse, r.spo2, r.temp, r.co2, r.rad);
  ramBuffer += line;
}

void flushToFlash() {
  if (!loggerReady || ramBuffer.length() == 0) return;

  // If the file is already past the maximum, delete it (see explanation above).
  if (LittleFS.exists(LOG_PATH)) {
    File existing = LittleFS.open(LOG_PATH, "r");
    size_t size = existing ? existing.size() : 0;
    if (existing) existing.close();
    if (size > MAX_FILE_SIZE) {
      LittleFS.remove(LOG_PATH);
      Serial.println("[logger] File full: deleted and starting again.");
    }
  }

  bool isNew = !LittleFS.exists(LOG_PATH);
  File file = LittleFS.open(LOG_PATH, "a");  // "a" = append (creates it if missing)
  if (!file) {
    Serial.println("[logger] Could not open the file. Retrying on the next flush.");
    return;  // The buffer is kept for the next attempt
  }
  if (isNew) file.print(CSV_HEADER);
  file.print(ramBuffer);
  file.close();
  ramBuffer = "";
}
