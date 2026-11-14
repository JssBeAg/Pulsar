// red.cpp — Implementación de WiFi + MQTT sin bloqueo.
// Contrato de datos: README, sección 9. Si se cambia, avisar al equipo.
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "red.h"
#include "config.h"

static const unsigned long INTERVALO_REINTENTO_MS = 5000;  // Cada cuánto se reintenta conectar
static const char* ID_CLIENTE_MQTT = "pulsar-astro1-sensor";
static const char* TOPICO_VITALES = "pulsar/astro1/vitales";
static const char* TOPICO_AMBIENTE = "pulsar/astro1/ambiente";

static WiFiClient clienteWifi;
static PubSubClient clienteMqtt(clienteWifi);

static unsigned long ultimoIntentoWifi = 0;
static unsigned long ultimoIntentoMqtt = 0;

void iniciarRed() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // Arranca el intento y regresa de inmediato
  ultimoIntentoWifi = millis();
  clienteMqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void mantenerRed() {
  unsigned long ahora = millis();

  // 1) Sin WiFi: reintentar cada 5 s y salir (sin WiFi no tiene caso intentar MQTT).
  if (WiFi.status() != WL_CONNECTED) {
    if (ahora - ultimoIntentoWifi >= INTERVALO_REINTENTO_MS) {
      ultimoIntentoWifi = ahora;
      Serial.println("[red] Sin WiFi, reintentando...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  }

  // 2) Hay WiFi pero no MQTT: reintentar cada 5 s.
  if (!clienteMqtt.connected()) {
    if (ahora - ultimoIntentoMqtt >= INTERVALO_REINTENTO_MS) {
      ultimoIntentoMqtt = ahora;
      Serial.println("[red] Conectando a MQTT...");
      // TODO: connect() abre una conexión TCP y puede tardar unos segundos si la
      // laptop no responde (timeout del WiFiClient). Medirlo en el hardware; si
      // congela la OLED, bajar el timeout del cliente.
      if (clienteMqtt.connect(ID_CLIENTE_MQTT)) {
        Serial.println("[red] MQTT conectado.");
      }
    }
    return;
  }

  // 3) Todo conectado: atender al cliente MQTT (mantiene viva la conexión).
  clienteMqtt.loop();
}

bool mqttConectado() {
  return clienteMqtt.connected();
}

void publicarVitales(const Lecturas& l) {
  if (!mqttConectado()) return;  // Sin red no se hace nada

  char json[96];
  snprintf(json, sizeof(json), "{\"pulso\":%.0f,\"spo2\":%.0f,\"temp\":%.1f}",
           l.pulso, l.spo2, l.temp);
  clienteMqtt.publish(TOPICO_VITALES, json);

  snprintf(json, sizeof(json), "{\"co2\":%.0f,\"rad\":%.2f}", l.co2, l.rad);
  clienteMqtt.publish(TOPICO_AMBIENTE, json);
}
