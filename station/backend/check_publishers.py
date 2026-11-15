"""Diagnostic: who is publishing on pulsar/+/status?

Listens for a fixed time, prints every message, then prints a per-topic
count. If one topic gets about twice the expected rate (one message every
2 s), more than one publisher is using the same astronaut ID.

Diagnostic tool only. It does not touch the database.

    python station/backend/check_publishers.py [--seconds 10] [--host localhost]
"""

import argparse
import os
import time
from collections import Counter
from datetime import datetime

import paho.mqtt.client as mqtt

STATUS_TOPIC = "pulsar/+/status"
MQTT_PORT = 1883
MQTT_QOS = 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="List the publishers on pulsar/+/status.")
    parser.add_argument("--seconds", type=float, default=10.0, help="how long to listen (default 10)")
    parser.add_argument("--host", default="localhost", help="MQTT broker host (default localhost)")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    counts: Counter[str] = Counter()

    def on_connect(client, userdata, flags, reason_code, properties) -> None:
        client.subscribe(STATUS_TOPIC, qos=MQTT_QOS)

    def on_message(client, userdata, message: mqtt.MQTTMessage) -> None:
        received = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        counts[message.topic] += 1
        payload = message.payload.decode("utf-8", errors="replace")
        print(f"{received}  {message.topic}  {payload}", flush=True)

    # A unique client ID, so this never kicks the ingestor or a sensor off the broker.
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"check-publishers-{os.getpid()}")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, MQTT_PORT)
    client.loop_start()
    print(f"listening on {STATUS_TOPIC} for {args.seconds:.0f} s...", flush=True)
    try:
        time.sleep(args.seconds)
    except KeyboardInterrupt:
        pass
    client.loop_stop()
    client.disconnect()

    print("--- summary ---")
    if not counts:
        print("no messages")
    for topic, count in sorted(counts.items()):
        print(f"{topic}: {count} messages")


if __name__ == "__main__":
    main()
