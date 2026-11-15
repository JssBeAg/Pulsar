// rules.h — Health rules: turn a number into a traffic light + action.
// Source of truth for the thresholds: docs/rules.md
// Keep in sync with station/backend/rules.py
#pragma once

// Order matters: GREEN < YELLOW < RED (least to most severe).
// worstLevel() relies on this order.
enum Level { GREEN, YELLOW, RED };

// Result of evaluating one variable.
struct Evaluation {
  Level level;         // Traffic light
  const char* system;  // Body or ship system (e.g. "Cardiovascular")
  const char* reason;  // Why it is at that level (e.g. "Pulse high")
  const char* action;  // What the astronaut should do
};

Evaluation evaluatePulse(float bpm);
Evaluation evaluateSpO2(float percent);
Evaluation evaluateTemp(float degrees);
Evaluation evaluateCO2(float ppm);

// Returns the most severe level in a list of evaluations (overall status).
Level worstLevel(const Evaluation evaluations[], int count);
