// registro.h — Registro local en la flash interna (LittleFS), archivo /registro.csv.
// Funciona sin red: es la "caja negra" del astronauta.
#pragma once
#include "lectura.h"

// Monta LittleFS (lo formatea si no se puede montar). Devuelve true si quedó listo.
bool iniciarRegistro();

// Agrega una línea al buffer en RAM (NO escribe en la flash).
// `ts` = segundos desde que arrancó el ESP32 (no tiene reloj real).
void agregarLinea(const Lecturas& l, unsigned long ts);

// Escribe el buffer de RAM en /registro.csv y lo vacía.
// Llamar cada 30 s, NO en cada lectura: la flash se desgasta con muchas escrituras pequeñas.
void volcarAFlash();
