// logger.h — CSV log on the internal flash (LittleFS), file LOG_PATH.
// Works without a network: it is the astronaut's local record.
#pragma once

// Mounts LittleFS (formatting it on first use) and creates LOG_PATH with the
// header "ms,screen" if it does not exist. Returns false if the mount fails.
bool initLogger();

// Appends one record "ms,screen" to LOG_PATH and prints it to serial.
void logEvent(unsigned long ms, const char* screen);

// Appends one record "ms,screen,published" to PUBLISH_LOG_PATH (published = 1
// or 0), creating the file with that header if needed. Silent on success:
// it runs on every heartbeat.
void logPublish(unsigned long ms, const char* screen, bool published);

// Prints the whole button log (LOG_PATH) to serial, between "--- log contents ---" and "--- end ---".
void dumpLog();
