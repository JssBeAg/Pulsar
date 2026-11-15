"""Pulsar ingestor: MQTT → SQLite (status events), plus acks back to the sensor.

The ONLY process that subscribes to MQTT. It stamps each message with the
station's clock and the astronaut ID from the topic, then writes it to
SQLite. Keeping MQTT here means the dashboard stays read-only.

It is also the only station publisher: after each HELLO/BYE is committed,
it publishes {"received_at": ..., "screen": ...} to pulsar/<id>/ack with the
same client, so the sensor knows the station really received its data.

Run from the repo root:

    python station/backend/ingestor.py

Environment: MQTT_HOST (default localhost), MQTT_PORT (default 1883),
PULSAR_DB (default data/pulsar.db, relative to the repo root).
"""

import json
import os
import signal
import sqlite3
import sys
import time

import paho.mqtt.client as mqtt

import db

STATUS_TOPIC = "pulsar/+/status"
# Subscription QoS is a maximum: the sensor publishes status at QoS 0
# (PubSubClient only publishes QoS 0), so its messages arrive at QoS 0.
MQTT_QOS = 1
# Acks go back at QoS 0: PubSubClient on the sensor does not send QoS 1
# acknowledgments for incoming messages, so it cannot handle QoS 1 reliably.
ACK_QOS = 0
# Screens a live sensor sends. OFFLINE is not acked: it is the Last Will,
# published by the broker when no sensor is listening.
ACKED_SCREENS = {"HELLO", "BYE"}
# OFFLINE is never sent by a live sensor: it is the sensor's Last Will, which
# the broker publishes on the sensor's behalf when its connection drops.
VALID_SCREENS = {"HELLO", "BYE", "OFFLINE"}

# The one piece of global mutable state: the ingestor's database connection.
con: sqlite3.Connection | None = None


def log_error(message: str) -> None:
    print(f"[ingestor] {message}", file=sys.stderr, flush=True)


def astronaut_from_topic(topic: str) -> str | None:
    """Returns the astronaut ID from pulsar/<astronaut_id>/status, or None if the topic is malformed."""
    parts = topic.split("/")
    if len(parts) != 3 or parts[0] != "pulsar" or parts[2] != "status" or not parts[1]:
        return None
    return parts[1]


def parse_payload(payload: bytes) -> tuple[str, int | None] | None:
    """Returns (screen, uptime_s) from the JSON payload, or None if it is invalid."""
    try:
        data = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict):
        return None
    screen = data.get("screen")
    if screen not in VALID_SCREENS:
        return None
    uptime = data.get("uptime_s")
    # uptime_s is optional; anything that is not a plain integer is ignored, not fatal.
    if not isinstance(uptime, int) or isinstance(uptime, bool):
        uptime = None
    return screen, uptime


def on_connect(client: mqtt.Client, userdata, flags, reason_code, properties) -> None:
    if reason_code.is_failure:
        log_error(f"broker refused the connection: {reason_code}")
        return
    # Subscribing in on_connect means paho re-subscribes after every reconnect.
    client.subscribe(STATUS_TOPIC, qos=MQTT_QOS)
    print(f"subscribed to {STATUS_TOPIC} (QoS {MQTT_QOS})", flush=True)


def on_disconnect(client: mqtt.Client, userdata, flags, reason_code, properties) -> None:
    if reason_code != 0:
        log_error(f"disconnected from broker ({reason_code}), reconnecting...")


def on_message(client: mqtt.Client, userdata, message: mqtt.MQTTMessage) -> None:
    # The station's clock stamps the message: the sensor has no real-time
    # clock, and the dashboard's link-lost decision compares against this.
    received_at = time.time()

    astronaut_id = astronaut_from_topic(message.topic)
    if astronaut_id is None:
        log_error(f"dropped: malformed topic {message.topic!r}")
        return
    parsed = parse_payload(message.payload)
    if parsed is None:
        log_error(f"dropped: invalid payload on {message.topic}: {message.payload!r}")
        return
    screen, uptime = parsed

    try:
        db.insert_status(con, received_at, astronaut_id, screen, uptime)
    except sqlite3.Error as error:
        # No ack: the sensor must not believe the station has data it lost.
        log_error(f"database insert failed for {astronaut_id} {screen}, not acked: {error}")
        return
    uptime_text = uptime if uptime is not None else "-"
    print(f"received {astronaut_id} {screen} uptime={uptime_text}", flush=True)

    if screen in ACKED_SCREENS:
        send_ack(client, astronaut_id, received_at, screen)


def send_ack(client: mqtt.Client, astronaut_id: str, received_at: float, screen: str) -> None:
    """Confirms a stored status message on pulsar/<astronaut_id>/ack.

    The ack travels back through the same path as the status (station
    network, EVA-LINK, sensor), so it only arrives when every link is up.
    Called after the commit, so an ack always means "stored in SQLite".
    """
    payload = json.dumps({"received_at": round(received_at, 3), "screen": screen})
    client.publish(f"pulsar/{astronaut_id}/ack", payload, qos=ACK_QOS, retain=False)
    print(f"ack {astronaut_id} {screen}", flush=True)


def build_client() -> mqtt.Client:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pulsar-ingestor")
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    # paho's own reconnect, with back-off, instead of a hand-written loop.
    client.reconnect_delay_set(min_delay=1, max_delay=10)
    return client


def main() -> None:
    global con
    host = os.environ.get("MQTT_HOST", "localhost")
    port = int(os.environ.get("MQTT_PORT", "1883"))

    con = db.connect_writer()
    print(f"database {db.db_path()}", flush=True)

    client = build_client()
    # Ctrl+C asks the client to disconnect; loop_forever then returns normally,
    # so the shutdown is clean and prints no traceback.
    signal.signal(signal.SIGINT, lambda signum, frame: client.disconnect())

    # connect_async + retry_first_connection: the ingestor may start before
    # the broker and simply keeps trying.
    client.connect_async(host, port)
    print(f"connecting to {host}:{port}...", flush=True)
    try:
        client.loop_forever(retry_first_connection=True)
    finally:
        con.close()
        print("ingestor stopped", flush=True)


if __name__ == "__main__":
    main()
