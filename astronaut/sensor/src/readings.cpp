// readings.cpp — Reading implementation (simulated with potentiometers).
// The ONLY file that needs to change when moving to real sensors.
#include <Arduino.h>
#include "readings.h"
#include "config.h"

// The ESP32 ADC is 12-bit: it returns values from 0 to 4095.
static const int ADC_MAX = 4095;

// Arduino's map() works with integers. We multiply the ranges by 100
// so decimals are not lost (for example, a temperature of 36.65 °C).
static const long DECIMAL_SCALE = 100;

// Range each potentiometer simulates (knob at minimum to knob at maximum).
static const float PULSE_MIN = 40.0f,  PULSE_MAX = 180.0f;   // bpm
static const float SPO2_MIN  = 85.0f,  SPO2_MAX  = 100.0f;   // %
static const float TEMP_MIN  = 35.0f,  TEMP_MAX  = 40.0f;    // °C
static const float CO2_MIN   = 400.0f, CO2_MAX   = 5000.0f;  // ppm
static const float RAD_MIN   = 0.0f,   RAD_MAX   = 10.0f;    // simulated unit

// Reads a potentiometer and converts its position to the range [minimum, maximum].
// Internal function: when switching to a real sensor, replace it here.
static float readPotentiometer(int pin, float minimum, float maximum) {
  int raw = analogRead(pin);  // 0..4095
  long scaled = map(raw, 0, ADC_MAX,
                    (long)(minimum * DECIMAL_SCALE),
                    (long)(maximum * DECIMAL_SCALE));
  return scaled / (float)DECIMAL_SCALE;
}

void initReadings() {
  analogReadResolution(12);        // 12 bits → 0..4095
  analogSetAttenuation(ADC_11db);  // Lets us read almost the full 0–3.3 V range
}

Readings readAll() {
  Readings r;
  r.pulse = readPotentiometer(PIN_PULSE, PULSE_MIN, PULSE_MAX);
  r.spo2  = readPotentiometer(PIN_SPO2,  SPO2_MIN,  SPO2_MAX);
  r.temp  = readPotentiometer(PIN_TEMP,  TEMP_MIN,  TEMP_MAX);
  r.co2   = readPotentiometer(PIN_CO2,   CO2_MIN,   CO2_MAX);
  r.rad   = readPotentiometer(PIN_RAD,   RAD_MIN,   RAD_MAX);
  return r;
}
