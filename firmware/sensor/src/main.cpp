// main.cpp — ESP32 sensor de Pulsar.
// Lee → evalúa → muestra → publica cada segundo, y guarda el registro cada 30 s.
// Funciona aunque no haya WiFi: la red nunca detiene el loop (sin delay ni while).
#include <Arduino.h>
#include "config.h"
#include "lectura.h"
#include "reglas.h"
#include "pantalla.h"
#include "registro.h"
#include "red.h"

static const int CANTIDAD_SISTEMAS = 4;  // Pulso, SpO2, temperatura, CO2

static unsigned long ultimaLectura = 0;
static unsigned long ultimoVolcado = 0;

void setup() {
  Serial.begin(115200);
  iniciarPantalla();
  iniciarLectura();
  iniciarRegistro();
  iniciarRed();
  Serial.println("[pulsar] Sensor listo.");
}

void loop() {
  unsigned long ahora = millis();

  // En cada vuelta: mantener la red (no bloquea).
  mantenerRed();

  // Cada 1 s: leer → evaluar → mostrar → publicar (y agregar al registro en RAM).
  if (ahora - ultimaLectura >= INTERVALO_MUESTREO_MS) {
    ultimaLectura = ahora;

    Lecturas l = leerTodo();

    Evaluacion evaluaciones[CANTIDAD_SISTEMAS] = {
      evaluarPulso(l.pulso),
      evaluarSpO2(l.spo2),
      evaluarTemp(l.temp),
      evaluarCO2(l.co2),
    };
    Nivel general = peorNivel(evaluaciones, CANTIDAD_SISTEMAS);

    // En la pantalla se muestra la primera evaluación con el peor nivel
    // (si hay empate, manda el orden de la lista: pulso, SpO2, temp, CO2).
    const Evaluacion* principal = &evaluaciones[0];
    for (int i = 0; i < CANTIDAD_SISTEMAS; i++) {
      if (evaluaciones[i].nivel == general) {
        principal = &evaluaciones[i];
        break;
      }
    }

    mostrarEstado(*principal, l);
    publicarVitales(l);
    agregarLinea(l, ahora / 1000);  // Segundos desde el arranque

    Serial.printf("[pulsar] pulso=%.0f spo2=%.0f temp=%.2f co2=%.0f rad=%.2f | %s: %s | mqtt=%s\n",
                  l.pulso, l.spo2, l.temp, l.co2, l.rad,
                  principal->sistema, principal->motivo,
                  mqttConectado() ? "si" : "no");
  }

  // Cada 30 s: escribir el registro en la flash.
  if (ahora - ultimoVolcado >= INTERVALO_GUARDADO_MS) {
    ultimoVolcado = ahora;
    volcarAFlash();
  }
}
