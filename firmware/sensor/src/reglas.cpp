// reglas.cpp — Implementación de las reglas de salud.
// Fuente de verdad de los umbrales: docs/reglas.md
// Mantener sincronizado con backend/reglas.py
//
// ⚠️ Valores de demostración, no son criterios médicos.
//
// Los textos van SIN acentos ni ñ (la fuente por defecto de Adafruit GFX no
// los dibuja bien) y con 21 caracteres o menos (lo que cabe en una línea de la OLED).
#include "reglas.h"

// ===== Umbrales del pulso (bpm) =====
static const float PULSO_ROJO_BAJO = 40.0f;   // < 40  → rojo
static const float PULSO_VERDE_MIN = 50.0f;   // 40–50 → amarillo
static const float PULSO_VERDE_MAX = 100.0f;  // 50–100 → verde
static const float PULSO_ROJO_ALTO = 130.0f;  // 100–130 → amarillo, > 130 → rojo

// ===== Umbrales de SpO2 (%) =====
static const float SPO2_ROJO_BAJO = 90.0f;   // < 90 → rojo
static const float SPO2_VERDE_MIN = 95.0f;   // 90–94 → amarillo, ≥ 95 → verde

// ===== Umbrales de temperatura (°C) =====
static const float TEMP_ROJO_BAJO = 35.5f;   // < 35.5 → rojo
static const float TEMP_VERDE_MIN = 36.0f;   // 35.5–36 → amarillo
static const float TEMP_VERDE_MAX = 37.5f;   // 36–37.5 → verde
static const float TEMP_ROJO_ALTO = 38.5f;   // 37.5–38.5 → amarillo, > 38.5 → rojo

// ===== Umbrales de CO2 (ppm) =====
static const float CO2_VERDE_MAX = 1000.0f;  // < 1000 → verde
static const float CO2_ROJO_ALTO = 2500.0f;  // 1000–2500 → amarillo, > 2500 → rojo

Evaluacion evaluarPulso(float bpm) {
  const char* sistema = "Cardiovascular";
  if (bpm < PULSO_ROJO_BAJO)  return {ROJO,     sistema, "Pulso muy bajo", "Reposo y aviso medico"};
  if (bpm < PULSO_VERDE_MIN)  return {AMARILLO, sistema, "Pulso bajo",     "Revisa sensor, reposa"};
  if (bpm <= PULSO_VERDE_MAX) return {VERDE,    sistema, "Pulso normal",   "Continua normal"};
  if (bpm <= PULSO_ROJO_ALTO) return {AMARILLO, sistema, "Pulso alto",     "Baja ritmo y respira"};
  return                             {ROJO,     sistema, "Pulso muy alto", "Detente y pide ayuda"};
}

Evaluacion evaluarSpO2(float porcentaje) {
  const char* sistema = "Respiratorio";
  if (porcentaje < SPO2_ROJO_BAJO) return {ROJO,     sistema, "SpO2 muy baja", "Oxigeno y pide ayuda"};
  if (porcentaje < SPO2_VERDE_MIN) return {AMARILLO, sistema, "SpO2 baja",     "Revisa soporte vital"};
  return                                  {VERDE,    sistema, "SpO2 normal",   "Continua normal"};
}

Evaluacion evaluarTemp(float grados) {
  const char* sistema = "Termico";
  if (grados < TEMP_ROJO_BAJO)  return {ROJO,     sistema, "Temp muy baja", "Abrigate y avisa"};
  if (grados < TEMP_VERDE_MIN)  return {AMARILLO, sistema, "Temp baja",     "Abrigate"};
  if (grados <= TEMP_VERDE_MAX) return {VERDE,    sistema, "Temp normal",   "Continua normal"};
  if (grados <= TEMP_ROJO_ALTO) return {AMARILLO, sistema, "Temp alta",     "Hidratate y reposa"};
  return                               {ROJO,     sistema, "Fiebre alta",   "Pide ayuda medica"};
}

Evaluacion evaluarCO2(float ppm) {
  const char* sistema = "Ambiente";
  if (ppm < CO2_VERDE_MAX)  return {VERDE,    sistema, "CO2 normal",    "Continua normal"};
  if (ppm <= CO2_ROJO_ALTO) return {AMARILLO, sistema, "CO2 elevado",   "Revisa depurador CO2"};
  return                           {ROJO,     sistema, "CO2 peligroso", "Mascarilla y alerta"};
}

Nivel peorNivel(const Evaluacion evaluaciones[], int cantidad) {
  Nivel peor = VERDE;
  for (int i = 0; i < cantidad; i++) {
    if (evaluaciones[i].nivel > peor) {
      peor = evaluaciones[i].nivel;
    }
  }
  return peor;
}

// TODO: regla de radiación (dosis acumulada vs. límite diario). El límite no está definido en docs/reglas.md.
// TODO: línea base personal (RF3, > 2 desviaciones → amarillo, guardada en NVS). Fuera del alcance de esta base.
