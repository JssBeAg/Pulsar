// logger.cpp — LittleFS CSV log.
#include <Arduino.h>
#include <LittleFS.h>
#include "logger.h"
#include "config.h"

static const char* LOG_HEADER = "ms,screen";

static bool loggerReady = false;

bool initLogger() {
  // begin(true) formats the partition if it cannot be mounted. That happens
  // on the very first boot, when the flash has never held a file system.
  loggerReady = LittleFS.begin(true);
  if (!loggerReady) {
    Serial.println("[logger] ERROR: could not mount LittleFS. No records will be saved.");
    return false;
  }

  if (!LittleFS.exists(LOG_PATH)) {
    File file = LittleFS.open(LOG_PATH, "w");
    if (!file) {
      Serial.printf("[logger] ERROR: could not create %s.\n", LOG_PATH);
      loggerReady = false;
      return false;
    }
    file.println(LOG_HEADER);
    file.close();
    Serial.printf("[logger] Created %s\n", LOG_PATH);
  }
  return true;
}

void logEvent(unsigned long ms, const char* screen) {
  if (!loggerReady) {
    Serial.printf("[logger] not ready, record NOT saved: %lu,%s\n", ms, screen);
    return;
  }

  // Append mode: each record is added at the end and the file is closed right
  // away. Earlier records are never rewritten, so a power loss can at most
  // lose the record being written, never the ones already saved.
  File file = LittleFS.open(LOG_PATH, "a");
  if (!file) {
    Serial.printf("[logger] ERROR: could not open %s. Record NOT saved: %lu,%s\n", LOG_PATH, ms, screen);
    return;
  }
  file.printf("%lu,%s\n", ms, screen);
  file.close();
  Serial.printf("[logger] saved %lu,%s\n", ms, screen);
}

static const char* PUBLISH_LOG_HEADER = "ms,screen,published";

void logPublish(unsigned long ms, const char* screen, bool published) {
  if (!loggerReady) return;  // initLogger already reported why

  // A separate file, so each CSV keeps one column layout.
  bool isNew = !LittleFS.exists(PUBLISH_LOG_PATH);
  File file = LittleFS.open(PUBLISH_LOG_PATH, "a");
  if (!file) {
    Serial.printf("[logger] ERROR: could not open %s. Record NOT saved: %lu,%s,%d\n",
                  PUBLISH_LOG_PATH, ms, screen, published ? 1 : 0);
    return;
  }
  if (isNew) file.println(PUBLISH_LOG_HEADER);
  file.printf("%lu,%s,%d\n", ms, screen, published ? 1 : 0);
  file.close();
}

void dumpLog() {
  Serial.println("--- log contents ---");
  if (!loggerReady) {
    Serial.println("(logger not ready)");
  } else {
    File file = LittleFS.open(LOG_PATH, "r");
    if (!file) {
      Serial.printf("(could not open %s)\n", LOG_PATH);
    } else {
      while (file.available()) {
        Serial.write(file.read());
      }
      file.close();
    }
  }
  Serial.println("--- end ---");
}
