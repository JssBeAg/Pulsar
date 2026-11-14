// main.cpp — ESP32 access point: "la red de la nave".
// Solo crea la red WiFi y mueve paquetes. No hace nada más.
// Desenchufarlo simula perder la comunicación.
#include <Arduino.h>
#include <WiFi.h>

static const char* NOMBRE_RED = "PULSAR-NAVE";
static const char* CLAVE_RED = "pulsar2026";

// IP del access point. La laptop usa la IP fija 192.168.4.10 en esta red.
static const IPAddress IP_AP(192, 168, 4, 1);
static const IPAddress PUERTA_ENLACE(192, 168, 4, 1);
static const IPAddress MASCARA(255, 255, 255, 0);

static const unsigned long INTERVALO_ESTADO_MS = 5000;  // Cada cuánto se imprime el estado
static unsigned long ultimoEstado = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IP_AP, PUERTA_ENLACE, MASCARA);
  bool creada = WiFi.softAP(NOMBRE_RED, CLAVE_RED);

  if (creada) {
    Serial.printf("[ap] Red \"%s\" creada. IP: %s\n", NOMBRE_RED, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[ap] ERROR: no se pudo crear la red.");
  }
}

void loop() {
  // Imprime cuántos dispositivos están conectados, sin usar delay.
  unsigned long ahora = millis();
  if (ahora - ultimoEstado >= INTERVALO_ESTADO_MS) {
    ultimoEstado = ahora;
    Serial.printf("[ap] IP %s | dispositivos conectados: %d\n",
                  WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum());
  }
}
