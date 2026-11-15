// rules.cpp — Health rules implementation.
// Source of truth for the thresholds: docs/rules.md
// Keep in sync with station/backend/rules.py
//
// ⚠️ Demo values, not medical criteria.
//
// Texts are plain ASCII (the default Adafruit GFX font cannot draw accented
// characters) and 21 characters or fewer (what fits on one OLED line).
#include "rules.h"

// ===== Pulse thresholds (bpm) =====
static const float PULSE_RED_LOW   = 40.0f;   // < 40   → red
static const float PULSE_GREEN_MIN = 50.0f;   // 40–50  → yellow
static const float PULSE_GREEN_MAX = 100.0f;  // 50–100 → green
static const float PULSE_RED_HIGH  = 130.0f;  // 100–130 → yellow, > 130 → red

// ===== SpO2 thresholds (%) =====
static const float SPO2_RED_LOW   = 90.0f;  // < 90 → red
static const float SPO2_GREEN_MIN = 95.0f;  // 90–94 → yellow, ≥ 95 → green

// ===== Temperature thresholds (°C) =====
static const float TEMP_RED_LOW   = 35.5f;  // < 35.5 → red
static const float TEMP_GREEN_MIN = 36.0f;  // 35.5–36 → yellow
static const float TEMP_GREEN_MAX = 37.5f;  // 36–37.5 → green
static const float TEMP_RED_HIGH  = 38.5f;  // 37.5–38.5 → yellow, > 38.5 → red

// ===== CO2 thresholds (ppm) =====
static const float CO2_GREEN_MAX = 1000.0f;  // < 1000 → green
static const float CO2_RED_HIGH  = 2500.0f;  // 1000–2500 → yellow, > 2500 → red

Evaluation evaluatePulse(float bpm) {
  const char* system = "Cardiovascular";
  if (bpm < PULSE_RED_LOW)    return {RED,    system, "Pulse very low",  "Rest, alert medic"};
  if (bpm < PULSE_GREEN_MIN)  return {YELLOW, system, "Pulse low",       "Check sensor, rest"};
  if (bpm <= PULSE_GREEN_MAX) return {GREEN,  system, "Pulse normal",    "Carry on"};
  if (bpm <= PULSE_RED_HIGH)  return {YELLOW, system, "Pulse high",      "Slow down, breathe"};
  return                             {RED,    system, "Pulse very high", "Stop and get help"};
}

Evaluation evaluateSpO2(float percent) {
  const char* system = "Respiratory";
  if (percent < SPO2_RED_LOW)   return {RED,    system, "SpO2 very low", "Use O2, get help"};
  if (percent < SPO2_GREEN_MIN) return {YELLOW, system, "SpO2 low",      "Check life support"};
  return                               {GREEN,  system, "SpO2 normal",   "Carry on"};
}

Evaluation evaluateTemp(float degrees) {
  const char* system = "Thermal";
  if (degrees < TEMP_RED_LOW)    return {RED,    system, "Temp very low", "Warm up, alert crew"};
  if (degrees < TEMP_GREEN_MIN)  return {YELLOW, system, "Temp low",      "Warm up"};
  if (degrees <= TEMP_GREEN_MAX) return {GREEN,  system, "Temp normal",   "Carry on"};
  if (degrees <= TEMP_RED_HIGH)  return {YELLOW, system, "Temp high",     "Hydrate and rest"};
  return                                {RED,    system, "High fever",    "Get medical help"};
}

Evaluation evaluateCO2(float ppm) {
  const char* system = "Environment";
  if (ppm < CO2_GREEN_MAX) return {GREEN,  system, "CO2 normal",    "Carry on"};
  if (ppm <= CO2_RED_HIGH) return {YELLOW, system, "CO2 elevated",  "Check CO2 scrubber"};
  return                          {RED,    system, "CO2 dangerous", "Mask on, alert crew"};
}

Level worstLevel(const Evaluation evaluations[], int count) {
  Level worst = GREEN;
  for (int i = 0; i < count; i++) {
    if (evaluations[i].level > worst) {
      worst = evaluations[i].level;
    }
  }
  return worst;
}

// TODO: radiation rule (accumulated dose vs. daily limit). The limit is not defined in docs/rules.md.
// TODO: personal baseline (RF3, > 2 deviations → yellow, stored in NVS). Out of scope for this base.
