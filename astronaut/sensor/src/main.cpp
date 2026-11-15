// main.cpp — Pulsar astronaut sensor: hello world.
// First step only: proves the ESP32 drives the OLED. Sensors, rules,
// logging and networking come in later steps (their files stay in src/
// but are excluded from the build by build_src_filter in platformio.ini).
#include <Arduino.h>
#include "display.h"

static const unsigned long ALIVE_INTERVAL_MS = 5000;  // How often "alive" is printed (ms)
static unsigned long lastAliveAt = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Pulsar astronaut sensor: hello world");

  // If the display is missing, keep running: the serial heartbeat still
  // shows the board is alive, which helps when debugging the wiring.
  if (!initDisplay()) {
    Serial.println("[main] Display not available, continuing without it.");
  }
  showHello();
}

void loop() {
  // millis() instead of delay(), so the loop never blocks.
  unsigned long now = millis();
  if (now - lastAliveAt >= ALIVE_INTERVAL_MS) {
    lastAliveAt = now;
    Serial.printf("alive %lu s\n", now / 1000);
  }
}
