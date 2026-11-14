"""Configuración del backend de Pulsar (ingestor y dashboard)."""

# Broker MQTT: Mosquitto corre en la misma laptop (Docker).
MQTT_HOST = "localhost"
MQTT_PORT = 1883

# Tópicos que publica el ESP32 sensor (contrato de datos: README, sección 9).
TOPICOS = ["pulsar/astro1/vitales", "pulsar/astro1/ambiente"]

# Base de datos SQLite. Ruta relativa a la raíz del proyecto (db.py la resuelve).
DB_PATH = "data/pulsar.db"

# Astronauta monitoreado (por ahora solo uno).
ASTRONAUTA = "astro1"
