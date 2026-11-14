// reglas.h — Reglas de salud: convierten un número en semáforo + acción.
// Fuente de verdad de los umbrales: docs/reglas.md
// Mantener sincronizado con backend/reglas.py
#pragma once

// El orden importa: VERDE < AMARILLO < ROJO (de menos a más grave).
// peorNivel() se apoya en este orden.
enum Nivel { VERDE, AMARILLO, ROJO };

// Resultado de evaluar una variable.
struct Evaluacion {
  Nivel nivel;          // Semáforo
  const char* sistema;  // Sistema del cuerpo o de la nave (ej. "Cardiovascular")
  const char* motivo;   // Por qué está en ese nivel (ej. "Pulso alto")
  const char* accion;   // Qué debe hacer el astronauta
};

Evaluacion evaluarPulso(float bpm);
Evaluacion evaluarSpO2(float porcentaje);
Evaluacion evaluarTemp(float grados);
Evaluacion evaluarCO2(float ppm);

// Devuelve el nivel más grave de una lista de evaluaciones (estado general).
Nivel peorNivel(const Evaluacion evaluaciones[], int cantidad);
