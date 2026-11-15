# Health rules (source of truth)

> ⚠️ **Demo** values, not medical criteria.

This file is the **source of truth** for the thresholds. If you change anything here, change it in these files too:

- `astronaut/sensor/src/rules.cpp`
- `station/backend/rules.py`

## Thresholds

| System | Variable | Green | Yellow | Red |
|---|---|---|---|---|
| Cardiovascular | Pulse (bpm) | 50–100 | 40–50 / 100–130 | <40 / >130 |
| Respiratory | SpO₂ (%) | ≥95 | 90–94 | <90 |
| Thermal | Temperature (°C) | 36–37.5 | 35.5–36 / 37.5–38.5 | <35.5 / >38.5 |
| Environment | CO₂ (ppm) | <1000 | 1000–2500 | >2500 |

### Values exactly on a boundary

This is how the code handles values that fall exactly on an edge:

- **Pulse:** 40 → yellow, 50 → green, 100 → green, 130 → yellow.
- **SpO₂:** 90 → yellow, 95 → green.
- **Temperature:** 35.5 → yellow, 36 → green, 37.5 → green, 38.5 → yellow.
- **CO₂:** 1000 → yellow, 2500 → yellow.

## What each rule returns

Each rule returns a **level, reason and action**. The texts are plain ASCII and at most 21 characters long, so they fit on one OLED line.

| Variable | Level | Reason | Action |
|---|---|---|---|
| Pulse | Red (low) | Pulse very low | Rest, alert medic |
| Pulse | Yellow (low) | Pulse low | Check sensor, rest |
| Pulse | Green | Pulse normal | Carry on |
| Pulse | Yellow (high) | Pulse high | Slow down, breathe |
| Pulse | Red (high) | Pulse very high | Stop and get help |
| SpO₂ | Red | SpO2 very low | Use O2, get help |
| SpO₂ | Yellow | SpO2 low | Check life support |
| SpO₂ | Green | SpO2 normal | Carry on |
| Temperature | Red (low) | Temp very low | Warm up, alert crew |
| Temperature | Yellow (low) | Temp low | Warm up |
| Temperature | Green | Temp normal | Carry on |
| Temperature | Yellow (high) | Temp high | Hydrate and rest |
| Temperature | Red (high) | High fever | Get medical help |
| CO₂ | Green | CO2 normal | Carry on |
| CO₂ | Yellow | CO2 elevated | Check CO2 scrubber |
| CO₂ | Red | CO2 dangerous | Mask on, alert crew |

**Overall status:** the worst level across all systems. On a tie, the OLED shows the first one in this order: pulse, SpO₂, temperature, CO₂.

## Pending

- **Radiation:** accumulated dose compared against a daily limit. *Limit to be defined.*
- **Personal baseline:** more than 2 deviations from *their* normal → yellow (README, section 10).
- **Behavioral health:** rules based on the check-in (e.g. 3 days in a row with high stress and poor sleep).
