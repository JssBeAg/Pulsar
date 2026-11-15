// main.cpp — ESP32 access point: the EVA link (comms/access-point).
//
// Simulates the short-range extravehicular (EVA) radio link that carries the
// astronaut's data to the lunar base station. The astronaut's sensor and the
// base station join the EVA-LINK WiFi network. This firmware only offers WiFi.
//
// The EVA link is not a satellite: there are no orbits or pass windows.
// Coverage is lost for physical reasons, all modeled here:
//   1. DISTANCE  — a potentiometer sets how far the astronaut is from the base.
//                  Beyond LINK_RANGE_M the signal is gone (hard limit).
//   2. FRINGE    — between FRINGE_START_M and LINK_RANGE_M the signal degrades
//                  gradually: shadows become more frequent, longer and more
//                  often long. `zone` (0..1) measures how deep into it we are.
//   3. SHADOW    — craters, rocks and base structures block the signal at
//                  random moments, for a random time (short or long).
//   4. MANUAL    — the operator can force a drop with the button, for the demo.
// After any drop, the radio needs a moment to re-acquire the signal before the
// link is usable again (acquisition delay).
//
//   link_up = !manual_drop && !in_shadow && range_ok && acquisition_done
//
// Everything is driven by millis(); there is no delay() anywhere, so the
// button, the distance and the shadows are always evaluated.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>
#include <math.h>

// ===== Network =====
static const char* AP_SSID = "EVA-LINK";                 // Network name the sensor and base station join
static const char* AP_PASSWORD = "pulsar2026";           // WPA2 password (needs 8+ characters)
static const IPAddress AP_IP(192, 168, 4, 1);            // Fixed AP address; the base station uses 192.168.4.10
static const IPAddress AP_GATEWAY(192, 168, 4, 1);       // The AP is its own gateway
static const IPAddress AP_SUBNET(255, 255, 255, 0);      // /24 network
static const int AP_CHANNEL = 1;                         // WiFi channel (1–13)
static const int AP_MAX_CLIENTS = 4;                     // Max simultaneous WiFi clients (count)

// ===== Pins =====
static const int BUTTON_PIN = 4;                         // Push button to GND (pressed = LOW)
static const int LED_PIN = 16;                           // Link LED through 220 Ω to GND (HIGH = on)
static const int DISTANCE_PIN = 34;                      // Distance potentiometer, ADC1, input-only pin

// ===== Distance model =====
static const float DISTANCE_SPAN_M = 3000.0f;            // Distance at full clockwise (m); 0 m fully counter-clockwise
static const float LINK_RANGE_M = 1200.0f;               // Hard coverage limit from the base (m)
static const float HYSTERESIS_PCT = 10.0f;               // Must come back this far inside the limit to recover (%)
static const int DISTANCE_SAMPLES = 16;                  // ADC samples averaged per reading (count)
static const int ADC_MAX = 4095;                         // 12-bit ADC full scale (counts)

// ===== Fringe zone (gradual signal loss before the hard limit) =====
static const float FRINGE_START_M = 800.0f;              // Distance where the signal starts to degrade (m)
static const float FRINGE_FACTOR = 0.9f;                 // How much the mean shadow gap shrinks at the limit (0–1)
static const float FRINGE_MIN_GAP_S = 5.0f;              // Refractory period after a shadow at the limit (s)
static const float FRINGE_LONG_BOOST = 0.4f;             // Extra long-shadow probability at the limit (0–1)

// ===== Shadow model (terrain and structures) =====
static const unsigned long SHADOW_TICK_MS = 100;         // How often the shadow model is evaluated (ms)
static const float SHADOW_MEAN_INTERVAL_S = 180.0f;      // Mean time between shadow arrivals outside the fringe (s)
static const float SHADOW_MIN_GAP_S = 60.0f;             // Refractory period after a shadow, outside the fringe (s)
static const float SHADOW_LONG_PROB = 0.2f;              // Probability that a shadow is long, outside the fringe (0–1)
static const float SHADOW_SHORT_MIN_S = 2.0f;            // Short shadow (rock, body turning): min duration (s)
static const float SHADOW_SHORT_MAX_S = 8.0f;            // Short shadow: max duration (s)
static const float SHADOW_LONG_MIN_S = 15.0f;            // Long shadow (crater, wall, ridge): min duration (s)
static const float SHADOW_LONG_MAX_S = 45.0f;            // Long shadow: max duration (s)

// ===== Radio re-acquisition =====
static const unsigned long ACQUIRE_MIN_MS = 1500;        // Shortest re-acquisition time after a drop (ms)
static const unsigned long ACQUIRE_MAX_MS = 3500;        // Longest re-acquisition time after a drop (ms)

// ===== Button =====
static const unsigned long DEBOUNCE_MS = 50;             // Input must be stable this long to count (ms)
static const unsigned long LONG_PRESS_MS = 2000;         // Hold at least this long to return to AUTO (ms)

// ===== Status output =====
static const unsigned long STATUS_INTERVAL_MS = 5000;    // How often the status line is printed (ms)

// ===== State =====
enum Mode { MODE_AUTO, MODE_MANUAL };
static Mode mode = MODE_AUTO;            // MANUAL once the operator has used the short press

// Inputs to the link state
static bool manualDrop = false;          // Set only by the button
static bool inShadow = false;            // Set only by the shadow model
static bool rangeOk = true;              // Hard limit with hysteresis
static bool acquisitionDone = true;      // True once re-acquisition after a drop has finished

// Output: the one variable that holds the physical link state
static bool linkUp = false;

// Distance and fringe
static float distanceM = 0.0f;           // Last averaged distance reading (m)
static float zone = 0.0f;                // Depth into the fringe: 0 = clear signal, 1 = at the hard limit

// Shadows
static unsigned long lastShadowTickAt = 0; // When the shadow model last ran a tick (millis)
static unsigned long shadowEndedAt = 0;    // When the last shadow ended; the refractory period counts from here (millis)
static unsigned long shadowEndsAt = 0;     // When the current shadow ends (millis)
static bool shadowIsLong = false;
static float shadowDurationS = 0.0f;

// Acquisition
static bool acquiring = false;           // A re-acquisition timer is running
static unsigned long acquireDoneAt = 0;  // Target timestamp when re-acquisition finishes (millis)

// Button
static int rawButton = HIGH;             // Last raw reading (may bounce)
static int stableButton = HIGH;          // Debounced button state
static unsigned long rawChangedAt = 0;   // When the raw reading last changed
static unsigned long pressStartedAt = 0; // When the current press began
static bool longPressHandled = false;    // A long press acts once, while still held

static unsigned long lastStatusAt = 0;

// ---------------------------------------------------------------------------
// Random helpers (hardware RNG, so every boot gives a different sequence)
// ---------------------------------------------------------------------------

// Uniform in (0, 1]. Never 0, so ln(u) is always finite.
static float randomUnit() {
  return ((float)esp_random() + 1.0f) / 4294967296.0f;
}

// Uniform in [minS, maxS] seconds, returned in milliseconds.
static unsigned long randomDurationMs(float minS, float maxS) {
  return (unsigned long)((minS + randomUnit() * (maxS - minS)) * 1000.0f);
}

// Uniform integer in [minMs, maxMs].
static unsigned long randomBetweenMs(unsigned long minMs, unsigned long maxMs) {
  return minMs + esp_random() % (maxMs - minMs + 1);
}

// Linear interpolation (named to avoid clashing with std::lerp in the core).
static float interpolate(float from, float to, float t) {
  return from + (to - from) * t;
}

// Mean time between shadow arrivals for the current `zone`.
// In the fringe the link margin shrinks, so smaller obstacles are enough to
// cut the signal: arrivals become more frequent as `zone` grows.
static float meanEffectiveS() {
  return SHADOW_MEAN_INTERVAL_S * (1.0f - zone * FRINGE_FACTOR);
}

// Refractory period for the current `zone`: after a shadow, the astronaut
// needs some time to walk out of the terrain that caused it before the next
// one can begin. Near the limit, any small obstacle is enough, so it shrinks.
static float minGapS() {
  return interpolate(SHADOW_MIN_GAP_S, FRINGE_MIN_GAP_S, zone);
}

static const char* modeName() {
  return mode == MODE_AUTO ? "AUTO" : "MANUAL";
}

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

// Brings the access point up or down. Only touches WiFi when the state changes.
void applyLink(bool up) {
  if (up == linkUp) return;
  if (up) {
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS);
    // Restarting the AP after softAPdisconnect() can bring back the default
    // address, so the config is reapplied every time to keep 192.168.4.1.
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  } else {
    // Drops every client and hides the SSID, like losing the radio signal.
    WiFi.softAPdisconnect(true);
  }
  linkUp = up;
}

void setupNetwork() {
  WiFi.mode(WIFI_AP);
  applyLink(true);
  Serial.printf("%s ready, IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ---------------------------------------------------------------------------
// Distance and fringe zone
// ---------------------------------------------------------------------------

// How deep into the fringe the astronaut is. A real radio does not go from
// perfect to nothing at one distance: the margin shrinks gradually, so
// `zone` rises linearly from 0 at FRINGE_START_M to 1 at LINK_RANGE_M.
void computeZone() {
  zone = constrain((distanceM - FRINGE_START_M) / (LINK_RANGE_M - FRINGE_START_M), 0.0f, 1.0f);
}

// Reads the potentiometer (averaged over 16 samples to remove ADC noise),
// updates `zone`, and applies the hard limit. Hysteresis: the link drops
// above LINK_RANGE_M but only recovers below LINK_RANGE_M × (1 − HYSTERESIS_PCT/100).
// Without this gap a value sitting right on the limit would make the link
// flicker on and off.
void readDistance() {
  long sum = 0;
  for (int i = 0; i < DISTANCE_SAMPLES; i++) {
    sum += analogRead(DISTANCE_PIN);
  }
  float raw = (float)sum / DISTANCE_SAMPLES;
  distanceM = raw / ADC_MAX * DISTANCE_SPAN_M;
  computeZone();

  float recoverM = LINK_RANGE_M * (1.0f - HYSTERESIS_PCT / 100.0f);
  if (rangeOk && distanceM > LINK_RANGE_M) {
    rangeOk = false;
    Serial.printf("[distance] OUT of range: %.0f m > %.0f m (zone %.0f%%)\n",
                  distanceM, LINK_RANGE_M, zone * 100.0f);
  } else if (!rangeOk && distanceM < recoverM) {
    rangeOk = true;
    Serial.printf("[distance] back IN range: %.0f m < %.0f m (zone %.0f%%)\n",
                  distanceM, recoverM, zone * 100.0f);
  }
}

// ---------------------------------------------------------------------------
// Shadows
// ---------------------------------------------------------------------------

// Continuous shadow model. It runs in every mode, including during a manual
// drop: the terrain does not care about the button. That way, when the
// operator ends a manual drop, the link reflects where the astronaut really
// is; if they are still behind a crater, it stays down.
//
// Shadows arrive as a Poisson process: blockages appear at random moments
// with no memory of the last one. Instead of sampling "time until the next
// shadow" once, the start probability is evaluated on every tick from the
// CURRENT zone, so moving the potentiometer changes the rate on the next
// tick, not at the next shadow. There is no forced maximum gap: the process
// is memoryless, and forcing a shadow would not match the physics.
void updateShadows() {
  unsigned long now = millis();

  // Ending is checked on every loop pass, so the stored end time is exact.
  if (inShadow && (long)(now - shadowEndsAt) >= 0) {
    inShadow = false;
    shadowEndedAt = now;
    Serial.printf("[shadow] END %s shadow (%.1f s, zone %.0f%%)\n",
                  shadowIsLong ? "long" : "short", shadowDurationS, zone * 100.0f);
  }

  // Starting is evaluated at a constant tick rate, so the probability per
  // tick matches dt. `+=` keeps the average rate exact if a loop pass runs late.
  if (now - lastShadowTickAt < SHADOW_TICK_MS) return;
  lastShadowTickAt += SHADOW_TICK_MS;

  if (inShadow) return;                                   // One shadow at a time
  if ((now - shadowEndedAt) / 1000.0f < minGapS()) return;  // Refractory period, recomputed every tick

  // P(at least one arrival in dt) for a Poisson process with this mean:
  // 1 − exp(−dt / mean). expm1f is the same formula, computed without
  // losing precision when the probability is tiny (~0.0006 at 180 s).
  float dtS = SHADOW_TICK_MS / 1000.0f;
  float pStart = -expm1f(-dtS / meanEffectiveS());
  if (randomUnit() >= pStart) return;

  // Near the limit even a ridge or a wall is enough to cut a weak signal
  // for a long time, so long shadows become more likely with `zone`.
  float longProb = min(1.0f, SHADOW_LONG_PROB + zone * FRINGE_LONG_BOOST);
  shadowIsLong = randomUnit() <= longProb;
  unsigned long durationMs = shadowIsLong
      ? randomDurationMs(SHADOW_LONG_MIN_S, SHADOW_LONG_MAX_S)
      : randomDurationMs(SHADOW_SHORT_MIN_S, SHADOW_SHORT_MAX_S);
  shadowDurationS = durationMs / 1000.0f;
  shadowEndsAt = now + durationMs;
  inShadow = true;
  Serial.printf("[shadow] START %s shadow, %.1f s (zone %.0f%%)\n",
                shadowIsLong ? "long" : "short", shadowDurationS, zone * 100.0f);
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

// INPUT_PULLUP because the button connects the pin to GND: idle = HIGH,
// pressed = LOW, no external resistor. Contacts bounce for a few ms, so the
// reading must be stable for DEBOUNCE_MS before it counts; otherwise one press
// could toggle the manual drop twice.
void handleButton() {
  unsigned long now = millis();
  int reading = digitalRead(BUTTON_PIN);

  if (reading != rawButton) {
    rawButton = reading;
    rawChangedAt = now;
  }

  if (now - rawChangedAt >= DEBOUNCE_MS && reading != stableButton) {
    stableButton = reading;

    if (stableButton == LOW) {
      // Falling edge: a press starts (counted once per press).
      pressStartedAt = now;
      longPressHandled = false;
    } else if (!longPressHandled) {
      // Released before LONG_PRESS_MS: short press toggles the manual drop.
      // There is no timer: the operator decides how long it lasts.
      if (mode != MODE_MANUAL) {
        mode = MODE_MANUAL;
        Serial.println("[mode] MANUAL");
      }
      manualDrop = !manualDrop;
      Serial.println(manualDrop ? "[manual] drop START" : "[manual] drop END");
    }
  }

  // Long press acts as soon as the hold reaches LONG_PRESS_MS, so the
  // operator knows when to let go.
  if (stableButton == LOW && !longPressHandled && now - pressStartedAt >= LONG_PRESS_MS) {
    longPressHandled = true;
    if (manualDrop) {
      manualDrop = false;
      Serial.println("[manual] drop END");
    }
    if (mode != MODE_AUTO) {
      mode = MODE_AUTO;
      Serial.println("[mode] AUTO");
    }
  }
}

// ---------------------------------------------------------------------------
// Link state
// ---------------------------------------------------------------------------

// Prints every cause that is currently keeping the link down.
static void printDropCauses() {
  char causes[128] = "";
  size_t len = 0;
  if (manualDrop) {
    len += snprintf(causes + len, sizeof(causes) - len, "MANUAL ");
  }
  if (inShadow) {
    len += snprintf(causes + len, sizeof(causes) - len, "SHADOW (%s) %.0f s ",
                    shadowIsLong ? "long" : "short", shadowDurationS);
  }
  if (!rangeOk) {
    len += snprintf(causes + len, sizeof(causes) - len, "DISTANCE %.0f m > %.0f m ",
                    distanceM, LINK_RANGE_M);
  }
  Serial.printf("[link] DOWN, cause: %s\n", causes);
}

// Combines the inputs into one link state:
//   link_up = !manual_drop && !in_shadow && range_ok && acquisition_done
// When the blocking conditions clear, the link does not come back at once:
// a real radio needs a moment to find and lock onto the signal again, so a
// random acquisition delay runs first (a target timestamp, not delay()).
// The LED stays off during it.
void updateLinkState() {
  unsigned long now = millis();
  bool conditionsOk = !manualDrop && !inShadow && rangeOk;

  if (!conditionsOk) {
    // Any blocking cause cancels acquisition; the next recovery starts over.
    acquisitionDone = false;
    acquiring = false;
  } else if (!acquisitionDone) {
    if (!acquiring) {
      unsigned long delayMs = randomBetweenMs(ACQUIRE_MIN_MS, ACQUIRE_MAX_MS);
      acquireDoneAt = now + delayMs;
      acquiring = true;
      Serial.printf("[link] acquiring signal (%lu ms)...\n", delayMs);
    } else if ((long)(now - acquireDoneAt) >= 0) {
      acquiring = false;
      acquisitionDone = true;
    }
  }

  bool shouldBeUp = conditionsOk && acquisitionDone;
  if (shouldBeUp != linkUp) {
    applyLink(shouldBeUp);
    if (shouldBeUp) {
      Serial.printf("[link] UP (mode=%s, distance=%.0f m, zone %.0f%%)\n",
                    modeName(), distanceM, zone * 100.0f);
    } else {
      printDropCauses();
    }
  }
}

// ---------------------------------------------------------------------------
// LED and status
// ---------------------------------------------------------------------------

// The LED shows the physical link state only. It ignores the cause, the mode
// and the fringe zone: a random shadow turns it off exactly like a manual
// drop, because to the astronaut and the base both look the same — no signal.
void updateLed() {
  digitalWrite(LED_PIN, linkUp ? HIGH : LOW);
}

// Prints mode, link state, distance, zone, mean shadow interval and (when up)
// the client count every 5 s.
void printStatus() {
  unsigned long now = millis();
  if (now - lastStatusAt < STATUS_INTERVAL_MS) return;
  lastStatusAt = now;

  const char* state = linkUp ? "UP" : (acquiring ? "ACQUIRING" : "DOWN");
  if (linkUp) {
    Serial.printf("[status] mode=%s, link=%s, distance=%.0f m, zone=%.0f%%, mean=%.0f s, clients=%d\n",
                  modeName(), state, distanceM, zone * 100.0f, meanEffectiveS(),
                  WiFi.softAPgetStationNum());
  } else {
    Serial.printf("[status] mode=%s, link=%s, distance=%.0f m, zone=%.0f%%, mean=%.0f s\n",
                  modeName(), state, distanceM, zone * 100.0f, meanEffectiveS());
  }
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  analogReadResolution(12);              // 0..4095
  analogSetAttenuation(ADC_11db);        // Read close to the full 0–3.3 V range

  setupNetwork();
  readDistance();
  // Boot counts as the end of a shadow, so the refractory period also
  // applies at startup and the link is stable while things connect.
  shadowEndedAt = millis();
  lastShadowTickAt = millis();
  updateLed();
}

void loop() {
  handleButton();
  readDistance();
  updateShadows();
  updateLinkState();
  updateLed();
  printStatus();
}
