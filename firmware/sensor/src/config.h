// config.h — Constantes de configuración del ESP32 sensor.
// Todo lo que se puede "ajustar" sin tocar la lógica vive aquí.
#pragma once

// ===== Red WiFi de la nave (la crea el ESP32 access point) =====
constexpr const char* WIFI_SSID = "PULSAR-NAVE";  // Nombre de la red
constexpr const char* WIFI_PASS = "pulsar2026";   // Contraseña de la red

// ===== Broker MQTT (Mosquitto en la laptop) =====
constexpr const char* MQTT_HOST = "192.168.4.10";  // IP fija de la laptop en la red de la nave
constexpr int MQTT_PORT = 1883;                    // Puerto estándar de MQTT

// ===== Pines de los potenciómetros (solo ADC1: ADC2 no funciona con WiFi encendido) =====
constexpr int PIN_PULSO = 34;  // Simula el pulso cardiaco
constexpr int PIN_SPO2  = 35;  // Simula la saturación de oxígeno
constexpr int PIN_TEMP  = 32;  // Simula la temperatura corporal
constexpr int PIN_CO2   = 33;  // Simula el CO2 de la cabina
constexpr int PIN_RAD   = 36;  // Simula la radiación

// ===== Pantalla OLED (I2C) =====
constexpr int PIN_OLED_SDA = 21;  // Línea de datos I2C
constexpr int PIN_OLED_SCL = 22;  // Línea de reloj I2C

// ===== Tiempos =====
constexpr unsigned long INTERVALO_MUESTREO_MS = 1000;   // Cada cuánto se lee, evalúa, muestra y publica
constexpr unsigned long INTERVALO_GUARDADO_MS = 30000;  // Cada cuánto se escribe el registro en la flash
