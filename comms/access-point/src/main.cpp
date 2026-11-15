// main.cpp — ESP32 access point: the EVA link (comms/access-point).
//
// Simulates the short-range extravehicular (EVA) radio link that carries the
// astronaut's data to the lunar base station. Two WiFi sides run at once
// (WIFI_AP_STA):
//
//   AP  side: the EVA-LINK network the astronaut's sensors join (192.168.4.x).
//   STA side: a client of the station network, where the laptop with the
//             MQTT broker lives (STATION_WIFI_SSID in config.h).
//
// NAPT (NAT) on the AP interface forwards the sensors' packets out through
// the STA interface, so a sensor reaches the laptop by its station IP.
//
// The only cause of link loss is a random SHADOW: craters, rocks and base
// structures block the signal at unpredictable moments, for a short or long
// time. A shadow takes down the AP side only: the station network stays up,
// so the broker sees the sensor's connection drop and publishes its Last Will.
// After a shadow, the radio needs a moment to re-acquire the signal
// before the link is usable again (acquisition delay).
//
//   link_up = !in_shadow && acquisition_done
//
// Everything is driven by millis(); there is no delay() anywhere, so the
// shadow model keeps running while the link is down.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>
#include <math.h>
#include "config.h"

// ===== Network =====
static const char* AP_SSID = "EVA-LINK";                 // Network name the sensor and base station join
static const char* AP_PASSWORD = "pulsar2026";           // WPA2 password (needs 8+ characters)
static const IPAddress AP_IP(192, 168, 4, 1);            // Fixed AP address on the EVA-LINK side
static const IPAddress AP_GATEWAY(192, 168, 4, 1);       // The AP is its own gateway
static const IPAddress AP_SUBNET(255, 255, 255, 0);      // /24 network
static const int AP_CHANNEL = 1;                         // WiFi channel (1–13); once STA connects, the radio follows the station network's channel
static const int AP_MAX_CLIENTS = 4;                     // Max simultaneous WiFi clients (count)

// ===== Pins =====
static const int LED_PIN = 16;                           // Link LED through 220 Ω to GND (HIGH = on)

// ===== Shadow model (terrain and structures) =====
static const unsigned long SHADOW_TICK_MS = 100;         // How often the shadow model is evaluated (ms)
static const float SHADOW_MEAN_INTERVAL_S = 180.0f;      // Mean time between shadow arrivals (s)
static const float SHADOW_MIN_GAP_S = 60.0f;             // Refractory period after a shadow (s)
static const float SHADOW_LONG_PROB = 0.2f;              // Probability that a shadow is long (0–1)
static const float SHADOW_SHORT_MIN_S = 2.0f;            // Short shadow (rock, body turning): min duration (s)
static const float SHADOW_SHORT_MAX_S = 8.0f;            // Short shadow: max duration (s)
static const float SHADOW_LONG_MIN_S = 15.0f;            // Long shadow (crater, wall, ridge): min duration (s)
static const float SHADOW_LONG_MAX_S = 45.0f;            // Long shadow: max duration (s)

// ===== Radio re-acquisition =====
static const unsigned long ACQUIRE_MIN_MS = 1500;        // Shortest re-acquisition time after a shadow (ms)
static const unsigned long ACQUIRE_MAX_MS = 3500;        // Longest re-acquisition time after a shadow (ms)

// ===== Status output =====
static const unsigned long STATUS_INTERVAL_MS = 5000;    // How often the status line is printed (ms)

// ===== State =====
static bool inShadow = false;              // Set only by the shadow model
static bool acquisitionDone = true;        // True once re-acquisition after a shadow has finished
static bool linkUp = false;                // The one variable that holds the physical link state (AP side)

// Station network (STA side)
static bool stationUp = false;                  // STA connected with an IP, at the last check
static unsigned long lastStationAttemptAt = 0;  // When the last STA connection attempt started (millis)
static bool naptOn = false;                     // NAPT enabled on the current AP interface

// Shadows
static unsigned long lastShadowTickAt = 0; // When the shadow model last ran a tick (millis)
static unsigned long shadowEndedAt = 0;    // When the last shadow ended; the refractory period counts from here (millis)
static unsigned long shadowEndsAt = 0;     // When the current shadow ends (millis)

// Acquisition
static bool acquiring = false;             // A re-acquisition timer is running
static unsigned long acquireDoneAt = 0;    // Target timestamp when re-acquisition finishes (millis)
static unsigned long acquireDelayMs = 0;   // Length of the current re-acquisition (ms)

static unsigned long lastStatusAt = 0;

// ---------------------------------------------------------------------------
// Random helpers (hardware RNG, so every boot gives a different sequence)
// ---------------------------------------------------------------------------

// Uniform in (0, 1]. Never 0, so it is always safe in probability tests.
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
    // With wifioff=true this turns off the AP side only (AP_STA -> STA): the
    // station connection stays up during the shadow.
    WiFi.softAPdisconnect(true);
    naptOn = false;  // The AP interface is gone; NAPT is re-enabled when it returns
  }
  linkUp = up;
}

void setupNetwork() {
  WiFi.mode(WIFI_AP_STA);
  // Our own timer (updateStation) is the one retry mechanism for STA.
  WiFi.setAutoReconnect(false);
  applyLink(true);
  Serial.printf("%s ready, IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  WiFi.begin(STATION_WIFI_SSID, STATION_WIFI_PASSWORD);  // Returns at once
  lastStationAttemptAt = millis();
  Serial.printf("station link: connecting to %s...\n", STATION_WIFI_SSID);
}

// Keeps the STA side connected to the station network. Never blocks: a
// failed attempt is retried every STATION_RETRY_MS from the loop.
void updateStation() {
  unsigned long now = millis();
  bool up = WiFi.status() == WL_CONNECTED;

  if (up != stationUp) {
    stationUp = up;
    if (up) {
      Serial.printf("station link: UP %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("station link: DOWN");
      lastStationAttemptAt = now;  // Wait one interval before the first retry
    }
  }

  if (!up && now - lastStationAttemptAt >= STATION_RETRY_MS) {
    lastStationAttemptAt = now;
    WiFi.disconnect();  // STA only: abandons a half-finished attempt, the AP is untouched
    WiFi.begin(STATION_WIFI_SSID, STATION_WIFI_PASSWORD);
  }
}

// NAPT on the AP interface: packets from 192.168.4.x leave through the STA
// interface with EVA-LINK's station IP as their source. It is enabled once
// the station is connected and the AP is up, and again every time the AP
// comes back after a shadow (softAPdisconnect tears the AP interface down).
// WiFi.AP.enableNAPT() is the core's wrapper around lwIP NAPT; it takes the
// lwIP core lock, which this core's thread-safety check requires.
void updateNapt() {
  if (naptOn || !linkUp || !stationUp) return;
  naptOn = WiFi.AP.enableNAPT(true);
  Serial.println(naptOn ? "NAPT: enabled on EVA-LINK" : "NAPT: enable FAILED, retrying");
}

// ---------------------------------------------------------------------------
// Shadows
// ---------------------------------------------------------------------------

// Shadows model terrain and structures blocking the signal. Where those
// obstacles appear along the astronaut's path is unpredictable, so shadows
// arrive as a Poisson process: random moments, with no memory of the last one.
// The start test runs on every tick, so the decision is always current, and
// there is no forced maximum gap: forcing a shadow would not match the physics.
void updateShadows() {
  unsigned long now = millis();

  // Ending is checked on every loop pass, so the stored end time is exact.
  if (inShadow && (long)(now - shadowEndsAt) >= 0) {
    inShadow = false;
    shadowEndedAt = now;
    Serial.println("shadow end");
  }

  if (now - lastShadowTickAt < SHADOW_TICK_MS) return;
  // dt is the real time since the last tick, so a late loop pass still gets
  // the right probability.
  float dtS = (now - lastShadowTickAt) / 1000.0f;
  lastShadowTickAt = now;

  if (inShadow) return;  // One shadow at a time
  // Refractory gap: right after a shadow the astronaut is still moving away
  // from the obstacle that caused it, so shadows cannot stack back to back.
  if ((now - shadowEndedAt) / 1000.0f < SHADOW_MIN_GAP_S) return;

  // P(at least one arrival in dt) = 1 − exp(−dt / mean). expm1f is the same
  // formula without losing precision when the probability is tiny (~0.0006).
  float pStart = -expm1f(-dtS / SHADOW_MEAN_INTERVAL_S);
  if (randomUnit() >= pStart) return;

  bool isLong = randomUnit() <= SHADOW_LONG_PROB;
  unsigned long durationMs = isLong
      ? randomDurationMs(SHADOW_LONG_MIN_S, SHADOW_LONG_MAX_S)
      : randomDurationMs(SHADOW_SHORT_MIN_S, SHADOW_SHORT_MAX_S);
  shadowEndsAt = now + durationMs;
  inShadow = true;
  Serial.printf("shadow start: %s, %.0f s\n", isLong ? "long" : "short", durationMs / 1000.0f);
}

// ---------------------------------------------------------------------------
// Link state
// ---------------------------------------------------------------------------

// Combines the inputs into one link state:
//   link_up = !in_shadow && acquisition_done
// When a shadow ends, the link does not come back at once: a real radio
// needs a moment to find and lock onto the signal again, so a random
// acquisition delay runs first (a target timestamp, not delay()).
// The LED stays off during it.
void updateLinkState() {
  unsigned long now = millis();

  if (inShadow) {
    // A shadow cancels any re-acquisition; the next recovery starts over.
    acquisitionDone = false;
    acquiring = false;
  } else if (!acquisitionDone) {
    if (!acquiring) {
      acquireDelayMs = randomBetweenMs(ACQUIRE_MIN_MS, ACQUIRE_MAX_MS);
      acquireDoneAt = now + acquireDelayMs;
      acquiring = true;
    } else if ((long)(now - acquireDoneAt) >= 0) {
      acquiring = false;
      acquisitionDone = true;
    }
  }

  bool shouldBeUp = !inShadow && acquisitionDone;
  if (shouldBeUp != linkUp) {
    applyLink(shouldBeUp);
    if (shouldBeUp) {
      Serial.printf("link: UP (acquisition %lu ms)\n", acquireDelayMs);
    } else {
      Serial.println("link: DOWN");
    }
  }
}

// ---------------------------------------------------------------------------
// LED and status
// ---------------------------------------------------------------------------

// The LED shows the physical link state only, and ignores the cause: a
// shadow and re-acquisition look the same to the astronaut and the base —
// no signal.
void updateLed() {
  digitalWrite(LED_PIN, linkUp ? HIGH : LOW);
}

// Prints the link state and (when up) the client count every 5 s.
void printStatus() {
  unsigned long now = millis();
  if (now - lastStatusAt < STATUS_INTERVAL_MS) return;
  lastStatusAt = now;

  const char* state = linkUp ? "UP" : (acquiring ? "ACQUIRING" : "DOWN");
  const char* station = stationUp ? "UP" : "DOWN";
  if (linkUp) {
    Serial.printf("[status] link=%s, clients=%d, station=%s, napt=%s\n",
                  state, WiFi.softAPgetStationNum(), station, naptOn ? "on" : "off");
  } else {
    Serial.printf("[status] link=%s, station=%s\n", state, station);
  }
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  setupNetwork();
  // Boot counts as the end of a shadow, so the refractory period also
  // applies at startup and the link is stable while things connect.
  shadowEndedAt = millis();
  lastShadowTickAt = millis();
  updateLed();
}

void loop() {
  updateShadows();
  updateLinkState();
  updateStation();
  updateNapt();
  updateLed();
  printStatus();
}
