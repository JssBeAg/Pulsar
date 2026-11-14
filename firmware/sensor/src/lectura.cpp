// lectura.cpp — Implementación de la lectura (simulada con potenciómetros).
// ÚNICO archivo que hay que cambiar al pasar a sensores reales.
#include <Arduino.h>
#include "lectura.h"
#include "config.h"

// El ADC del ESP32 es de 12 bits: devuelve valores de 0 a 4095.
static const int ADC_MAXIMO = 4095;

// map() de Arduino trabaja con enteros. Multiplicamos los rangos por 100
// para no perder decimales (por ejemplo, temperatura 36.65 °C).
static const long ESCALA_DECIMALES = 100;

// Rangos que simula cada potenciómetro (de perilla al mínimo a perilla al máximo).
static const float PULSO_MIN = 40.0f,  PULSO_MAX = 180.0f;   // bpm
static const float SPO2_MIN  = 85.0f,  SPO2_MAX  = 100.0f;   // %
static const float TEMP_MIN  = 35.0f,  TEMP_MAX  = 40.0f;    // °C
static const float CO2_MIN   = 400.0f, CO2_MAX   = 5000.0f;  // ppm
static const float RAD_MIN   = 0.0f,   RAD_MAX   = 10.0f;    // unidad simulada

// Lee un potenciómetro y convierte su posición al rango [minimo, maximo].
// Función interna: si se cambia a un sensor real, se reemplaza aquí.
static float leerPotenciometro(int pin, float minimo, float maximo) {
  int crudo = analogRead(pin);  // 0..4095
  long escalado = map(crudo, 0, ADC_MAXIMO,
                      (long)(minimo * ESCALA_DECIMALES),
                      (long)(maximo * ESCALA_DECIMALES));
  return escalado / (float)ESCALA_DECIMALES;
}

void iniciarLectura() {
  analogReadResolution(12);        // 12 bits → 0..4095
  analogSetAttenuation(ADC_11db);  // Permite leer casi todo el rango 0–3.3 V
}

Lecturas leerTodo() {
  Lecturas l;
  l.pulso = leerPotenciometro(PIN_PULSO, PULSO_MIN, PULSO_MAX);
  l.spo2  = leerPotenciometro(PIN_SPO2,  SPO2_MIN,  SPO2_MAX);
  l.temp  = leerPotenciometro(PIN_TEMP,  TEMP_MIN,  TEMP_MAX);
  l.co2   = leerPotenciometro(PIN_CO2,   CO2_MIN,   CO2_MAX);
  l.rad   = leerPotenciometro(PIN_RAD,   RAD_MIN,   RAD_MAX);
  return l;
}
