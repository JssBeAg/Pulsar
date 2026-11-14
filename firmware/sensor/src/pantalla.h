// pantalla.h — Pantalla OLED SSD1306 (128x64, I2C, dirección 0x3C).
#pragma once
#include "lectura.h"
#include "reglas.h"

// Inicia I2C y la pantalla. Si la pantalla no responde, el resto del
// sistema sigue funcionando (solo se avisa por serial).
void iniciarPantalla();

// Muestra el estado general (VERDE/AMARILLO/ROJO en grande), la acción
// en una línea y la gráfica del pulso. También guarda el pulso en el historial.
// `ev` debe ser la evaluación con el peor nivel (la que manda).
void mostrarEstado(const Evaluacion& ev, const Lecturas& l);

// Dibuja la gráfica de línea de los últimos 100 valores de pulso en la
// parte baja de la pantalla. Solo dibuja en memoria: mostrarEstado() la
// llama y luego envía todo a la pantalla.
void dibujarGrafica();
