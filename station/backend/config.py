"""Configuration for the Pulsar station (ingestor and dashboard)."""

from pathlib import Path

# MQTT broker: Mosquitto runs on the same laptop (Docker).
MQTT_HOST = "localhost"
MQTT_PORT = 1883

# Topic patterns (data contract: README, section 9).
# "+" matches any single level, so one subscription covers every astronaut:
# pulsar/astro1/vitals, pulsar/astro2/vitals, ...
TOPICS = ["pulsar/+/vitals", "pulsar/+/environment"]
MQTT_QOS = 1

# SQLite database. Built from this file's location (station/backend/config.py),
# not the current directory, so it always points to <repo>/data/pulsar.db.
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DB_PATH = REPO_ROOT / "data" / "pulsar.db"
