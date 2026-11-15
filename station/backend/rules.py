"""Pulsar health rules.

Source of truth: docs/rules.md; keep in sync with astronaut/sensor/src/rules.cpp

Each function returns (level, reason, action), with level = "GREEN" | "YELLOW" | "RED".
The texts are the same as in the firmware (plain ASCII, at most 21 characters,
because of the OLED) so the dashboard and the display say exactly the same thing.

⚠️ Demo values, not medical criteria.
"""

GREEN = "GREEN"
YELLOW = "YELLOW"
RED = "RED"

# ===== Pulse thresholds (bpm) =====
PULSE_RED_LOW = 40.0     # < 40 → red
PULSE_GREEN_MIN = 50.0   # 40–50 → yellow
PULSE_GREEN_MAX = 100.0  # 50–100 → green
PULSE_RED_HIGH = 130.0   # 100–130 → yellow, > 130 → red

# ===== SpO2 thresholds (%) =====
SPO2_RED_LOW = 90.0    # < 90 → red
SPO2_GREEN_MIN = 95.0  # 90–94 → yellow, ≥ 95 → green

# ===== Temperature thresholds (°C) =====
TEMP_RED_LOW = 35.5    # < 35.5 → red
TEMP_GREEN_MIN = 36.0  # 35.5–36 → yellow
TEMP_GREEN_MAX = 37.5  # 36–37.5 → green
TEMP_RED_HIGH = 38.5   # 37.5–38.5 → yellow, > 38.5 → red

# ===== CO2 thresholds (ppm) =====
CO2_GREEN_MAX = 1000.0  # < 1000 → green
CO2_RED_HIGH = 2500.0   # 1000–2500 → yellow, > 2500 → red


def evaluate_pulse(bpm):
    if bpm < PULSE_RED_LOW:
        return RED, "Pulse very low", "Rest, alert medic"
    if bpm < PULSE_GREEN_MIN:
        return YELLOW, "Pulse low", "Check sensor, rest"
    if bpm <= PULSE_GREEN_MAX:
        return GREEN, "Pulse normal", "Carry on"
    if bpm <= PULSE_RED_HIGH:
        return YELLOW, "Pulse high", "Slow down, breathe"
    return RED, "Pulse very high", "Stop and get help"


def evaluate_spo2(percent):
    if percent < SPO2_RED_LOW:
        return RED, "SpO2 very low", "Use O2, get help"
    if percent < SPO2_GREEN_MIN:
        return YELLOW, "SpO2 low", "Check life support"
    return GREEN, "SpO2 normal", "Carry on"


def evaluate_temp(degrees):
    if degrees < TEMP_RED_LOW:
        return RED, "Temp very low", "Warm up, alert crew"
    if degrees < TEMP_GREEN_MIN:
        return YELLOW, "Temp low", "Warm up"
    if degrees <= TEMP_GREEN_MAX:
        return GREEN, "Temp normal", "Carry on"
    if degrees <= TEMP_RED_HIGH:
        return YELLOW, "Temp high", "Hydrate and rest"
    return RED, "High fever", "Get medical help"


def evaluate_co2(ppm):
    if ppm < CO2_GREEN_MAX:
        return GREEN, "CO2 normal", "Carry on"
    if ppm <= CO2_RED_HIGH:
        return YELLOW, "CO2 elevated", "Check CO2 scrubber"
    return RED, "CO2 dangerous", "Mask on, alert crew"


# TODO: radiation rule (accumulated dose vs. daily limit); the limit is not defined.
# TODO: long-term and check-in rules (RF8, e.g. 3 days of high stress + poor sleep).
