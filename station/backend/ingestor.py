"""Pulsar ingestor: MQTT → SQLite.

It is the ONLY program that writes readings and evaluations to the database.
Run from the repo root:

    python station/backend/ingestor.py
"""

import json
import time

import paho.mqtt.client as mqtt

import config
import db
import rules

# Which variables are evaluated, which system they belong to, and which rule applies.
# "rad" is saved but not evaluated yet (see TODO in rules.py).
EVALUATORS = {
    "pulse": ("Cardiovascular", rules.evaluate_pulse),
    "spo2": ("Respiratory", rules.evaluate_spo2),
    "temp": ("Thermal", rules.evaluate_temp),
    "co2": ("Environment", rules.evaluate_co2),
}


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"[ingestor] Connection refused by the broker: {reason_code}")
        return
    print(f"[ingestor] Connected to {config.MQTT_HOST}:{config.MQTT_PORT}")
    # Subscribing here means it re-subscribes automatically after reconnecting.
    for topic in config.TOPICS:
        client.subscribe(topic, qos=config.MQTT_QOS)
        print(f"[ingestor] Subscribed to {topic} (QoS {config.MQTT_QOS})")


def on_disconnect(client, userdata, flags, reason_code, properties):
    print(f"[ingestor] Disconnected ({reason_code}). Retrying...")


def astronaut_from_topic(topic):
    """Gets the astronaut ID from a topic like pulsar/<astronaut_id>/vitals.

    Returns None if the topic does not have that shape.
    """
    parts = topic.split("/")
    if len(parts) != 3 or parts[0] != "pulsar" or not parts[1]:
        return None
    return parts[1]


def on_message(client, userdata, message):
    # The ingestor sets the time: the ESP32 has no clock.
    ts = int(time.time())

    astronaut = astronaut_from_topic(message.topic)
    if astronaut is None:
        print(f"[ingestor] Unexpected topic, ignored: {message.topic}")
        return

    try:
        data = json.loads(message.payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        print(f"[ingestor] Invalid message on {message.topic}: {message.payload!r}")
        return
    if not isinstance(data, dict):
        print(f"[ingestor] Expected a JSON object on {message.topic}")
        return

    for variable, value in data.items():
        try:
            value = float(value)
        except (TypeError, ValueError):
            print(f"[ingestor] Non-numeric value: {variable}={value!r}")
            continue

        db.save_reading(ts, astronaut, variable, value)

        if variable in EVALUATORS:
            system, evaluate = EVALUATORS[variable]
            level, reason, action = evaluate(value)
            db.save_evaluation(ts, astronaut, system, level, reason, action)


def main():
    db.create_tables()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pulsar-ingestor")
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=10)

    # connect_async does not fail if the broker is not up yet;
    # loop_forever(retry_first_connection=True) keeps trying until it connects.
    client.connect_async(config.MQTT_HOST, config.MQTT_PORT)
    print(f"[ingestor] Saving to {config.DB_PATH}")
    try:
        client.loop_forever(retry_first_connection=True)
    except KeyboardInterrupt:
        print("\n[ingestor] Stopped.")
        client.disconnect()


if __name__ == "__main__":
    main()
