// registro.cpp — Implementación del registro CSV en LittleFS.
#include <Arduino.h>
#include <LittleFS.h>
#include "registro.h"

static const char* RUTA_REGISTRO = "/registro.csv";
static const char* ENCABEZADO_CSV = "ts,pulso,spo2,temp,co2,rad\n";

// Tamaño máximo del archivo: ~1 MB.
// La partición de LittleFS del esp32dev tiene ~1.4 MB. Si el archivo llenara
// la partición, las escrituras empezarían a fallar y dejaríamos de guardar
// justo lo más reciente (lo que más importa). LittleFS además necesita
// bloques libres para funcionar bien. Por eso, al pasar de ~1 MB, se borra
// y se empieza de nuevo: preferimos perder lo viejo que dejar de registrar.
static const size_t TAMANO_MAXIMO_ARCHIVO = 1024UL * 1024UL;

// Límite del buffer en RAM. Con 1 línea por segundo y volcado cada 30 s
// se usan ~1.5 KB; el límite solo protege si la flash falla varias veces seguidas.
static const size_t TAMANO_MAXIMO_BUFFER = 8 * 1024;

static String bufferRam;
static bool registroListo = false;

bool iniciarRegistro() {
  // begin(true) = formatear si no se puede montar (por ejemplo, la primera vez).
  registroListo = LittleFS.begin(true);
  bufferRam.reserve(2048);
  if (!registroListo) {
    Serial.println("[registro] No se pudo montar LittleFS. No se guardara registro.");
  }
  return registroListo;
}

void agregarLinea(const Lecturas& l, unsigned long ts) {
  if (bufferRam.length() >= TAMANO_MAXIMO_BUFFER) {
    return;  // Buffer lleno (la flash no está aceptando datos): se descarta la línea
  }
  char linea[96];
  snprintf(linea, sizeof(linea), "%lu,%.0f,%.0f,%.2f,%.0f,%.2f\n",
           ts, l.pulso, l.spo2, l.temp, l.co2, l.rad);
  bufferRam += linea;
}

void volcarAFlash() {
  if (!registroListo || bufferRam.length() == 0) return;

  // Si el archivo ya pasó del máximo, se borra (ver explicación arriba).
  if (LittleFS.exists(RUTA_REGISTRO)) {
    File existente = LittleFS.open(RUTA_REGISTRO, "r");
    size_t tamano = existente ? existente.size() : 0;
    if (existente) existente.close();
    if (tamano > TAMANO_MAXIMO_ARCHIVO) {
      LittleFS.remove(RUTA_REGISTRO);
      Serial.println("[registro] Archivo lleno: se borro y se empieza de nuevo.");
    }
  }

  bool esNuevo = !LittleFS.exists(RUTA_REGISTRO);
  File archivo = LittleFS.open(RUTA_REGISTRO, "a");  // "a" = agregar al final (lo crea si no existe)
  if (!archivo) {
    Serial.println("[registro] No se pudo abrir el archivo. Se reintenta en el siguiente volcado.");
    return;  // El buffer se conserva para el siguiente intento
  }
  if (esNuevo) archivo.print(ENCABEZADO_CSV);
  archivo.print(bufferRam);
  archivo.close();
  bufferRam = "";
}
