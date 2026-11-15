// main.cpp — Pulsar ESP32 sensor.
// Read → evaluate → display → publish every second, and save the log every 30 s.
// Works even with no WiFi: the network never stops the loop (no delay, no while).
#include <Arduino.h>
#include "config.h"
#include "readings.h"
#include "rules.h"
#include "display.h"
#include "logger.h"
#include "network.h"

static const int SYSTEM_COUNT = 4;  // Pulse, SpO2, temperature, CO2

static unsigned long lastSample = 0;
static unsigned long lastFlush = 0;

void setup() {
  Serial.begin(115200);
  initDisplay();
  initReadings();
  initLogger();
  initNetwork();
  Serial.println("[pulsar] Sensor ready.");
}

void loop() {
  unsigned long now = millis();

  // Every pass: maintain the network (non-blocking).
  maintainNetwork();

  // Every 1 s: read → evaluate → display → publish (and add to the RAM log).
  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;

    Readings r = readAll();

    Evaluation evaluations[SYSTEM_COUNT] = {
      evaluatePulse(r.pulse),
      evaluateSpO2(r.spo2),
      evaluateTemp(r.temp),
      evaluateCO2(r.co2),
    };
    Level overall = worstLevel(evaluations, SYSTEM_COUNT);

    // The display shows the first evaluation with the worst level
    // (on a tie, list order wins: pulse, SpO2, temp, CO2).
    const Evaluation* primary = &evaluations[0];
    for (int i = 0; i < SYSTEM_COUNT; i++) {
      if (evaluations[i].level == overall) {
        primary = &evaluations[i];
        break;
      }
    }

    showStatus(*primary, r);
    publishVitals(r);
    addLine(r, now / 1000);  // Seconds since boot

    Serial.printf("[pulsar] pulse=%.0f spo2=%.0f temp=%.2f co2=%.0f rad=%.2f | %s: %s | mqtt=%s\n",
                  r.pulse, r.spo2, r.temp, r.co2, r.rad,
                  primary->system, primary->reason,
                  mqttConnected() ? "yes" : "no");
  }

  // Every 30 s: write the log to flash.
  if (now - lastFlush >= FLUSH_INTERVAL_MS) {
    lastFlush = now;
    flushToFlash();
  }
}
