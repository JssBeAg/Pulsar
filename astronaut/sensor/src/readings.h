// readings.h — Reading vital signs and the environment.
// Today they are simulated with potentiometers. To use real sensors, only
// readings.cpp needs to change: the rest of the system only sees the Readings struct.
#pragma once

// A "snapshot" of every variable at one instant.
struct Readings {
  float pulse;  // Heart rate (bpm)
  float spo2;   // Oxygen saturation (%)
  float temp;   // Body temperature (°C)
  float co2;    // Cabin CO2 (ppm)
  float rad;    // Radiation (simulated unit, 0–10)
};

// Configures the ADC. Call once in setup().
void initReadings();

// Reads every variable and returns them already converted to their units.
Readings readAll();
