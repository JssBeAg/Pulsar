// lectura.h — Lectura de los signos vitales y del ambiente.
// Hoy se simulan con potenciómetros. Para usar sensores reales solo hay
// que cambiar lectura.cpp: el resto del sistema solo ve el struct Lecturas.
#pragma once

// Una "foto" de todas las variables en un instante.
struct Lecturas {
  float pulso;  // Pulso cardiaco (bpm)
  float spo2;   // Saturación de oxígeno (%)
  float temp;   // Temperatura corporal (°C)
  float co2;    // CO2 de la cabina (ppm)
  float rad;    // Radiación (unidad simulada, 0–10)
};

// Configura el ADC. Llamar una vez en setup().
void iniciarLectura();

// Lee todas las variables y las devuelve ya convertidas a sus unidades.
Lecturas leerTodo();
