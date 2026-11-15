// main.cpp — Pulsar astronaut sensor: button test.
// The push button is the only data source for now; it temporarily stands in
// for the potentiometers. Each press toggles the screen (HELLO/BYE) and
// writes one record to the flash log. The sensor also joins EVA-LINK when
// coverage allows, without ever waiting for it, and publishes the current
// screen to the station's MQTT broker on each press and every HEARTBEAT_MS.
// The dot in the top-right corner is filled while the station acknowledges
// that data (see mqttLive), and outlined when the acks stop.
#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "logger.h"
#include "mqtt.h"
#include "network.h"

// ===== State =====
static Screen currentScreen = SCREEN_HELLO;  // HELLO is the default at boot
static int lastButtonState = HIGH;           // Last accepted pin state (HIGH = released)
static unsigned long lastChangeAt = 0;       // When the last accepted change happened (millis)
static unsigned long pressCount = 0;         // Presses since boot
static unsigned long lastStatusAt = 0;
static bool indicatorShown = false;          // State the link dot currently shows (false = outlined)
static unsigned long lastIndicatorAt = 0;    // When the dot state was last checked (millis)

static const char* screenName(Screen screen) {
  return screen == SCREEN_HELLO ? "HELLO" : "BYE";
}

// Reads the button; on each new press toggles the screen and logs it.
static void handleButton() {
  unsigned long now = millis();

  // Mechanical contacts bounce for a few milliseconds when they close and
  // again when they open. Ignoring the pin for DEBOUNCE_MS after every
  // accepted change absorbs that, so one press is one record.
  if (now - lastChangeAt < DEBOUNCE_MS) return;

  int state = digitalRead(BUTTON_PIN);
  if (state == lastButtonState) return;

  lastButtonState = state;
  lastChangeAt = now;

  // Act only on released (HIGH) → pressed (LOW). Holding the button keeps it
  // LOW, so a long press is still one record.
  if (state == LOW) {
    currentScreen = (currentScreen == SCREEN_HELLO) ? SCREEN_BYE : SCREEN_HELLO;
    showScreen(currentScreen);
    pressCount++;
    logEvent(now, screenName(currentScreen));
    publishScreen(screenName(currentScreen));
  }
}

// Updates the link dot from the station's acks. Checked at most every
// INDICATOR_REFRESH_MS and redrawn only when the state changes, because each
// redraw sends the whole frame over I2C.
static void updateIndicator() {
  unsigned long now = millis();
  if (now - lastIndicatorAt < INDICATOR_REFRESH_MS) return;
  lastIndicatorAt = now;

  bool live = mqttLive();
  if (live == indicatorShown) return;
  indicatorShown = live;
  drawLinkIndicator(live);
  Serial.println(live ? "[main] station acks: LIVE (dot on)" : "[main] station acks: none (dot off)");
}

// Prints a heartbeat with the link state and the number of presses.
static void printStatus() {
  unsigned long now = millis();
  if (now - lastStatusAt < STATUS_INTERVAL_MS) return;
  lastStatusAt = now;
  Serial.printf("alive %lu s, link: %s, mqtt: %s, acks: %s, records: %lu\n",
                now / 1000, networkUp() ? "UP" : "DOWN", mqttUp() ? "UP" : "DOWN",
                mqttLive() ? "LIVE" : "none", pressCount);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Pulsar astronaut sensor: button test");

  // INPUT_PULLUP: the internal pull-up holds the pin HIGH, and the button
  // pulls it to GND, so pressed reads LOW and no external resistor is needed.
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Each step reports its own error and the sensor keeps running: the
  // button, the screen and the log do not depend on each other.
  if (!initLogger()) {
    Serial.println("[main] Logger not available, continuing without it.");
  }
  dumpLog();  // Shows the records saved before this boot

  if (!initDisplay()) {
    Serial.println("[main] Display not available, continuing without it.");
  }
  drawLinkIndicator(false);  // Outlined until the first ack arrives
  showScreen(currentScreen);

  startNetwork();
  startMqtt();
}

void loop() {
  // millis() everywhere, never delay(): nothing in the loop waits on purpose.
  maintainNetwork();
  maintainMqtt(networkUp(), screenName(currentScreen));
  handleButton();
  updateIndicator();
  printStatus();
}
