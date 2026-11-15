// mqtt.h — Publishes the sensor's screen to the station's MQTT broker and
// listens for the station's acknowledgments.
//
// Status:  pulsar/<ASTRONAUT_ID>/status, {"screen": "HELLO", "uptime_s": 123}
//          QoS 0: PubSubClient only publishes at QoS 0 (the earlier spec said
//          QoS 1). A lost message is covered by the next heartbeat, and the
//          missing ack shows it on the dot.
// Ack:     pulsar/<ASTRONAUT_ID>/ack, {"received_at": ..., "screen": ...}
//          Published by the station's ingestor after it stores each status.
//          Subscribed at QoS 0.
// Last Will (sent by the broker if our connection drops):
//          {"screen": "OFFLINE"}, QoS 1, not retained
//
// MQTT is optional: nothing here makes the button, the screen or the log
// wait for the broker. Every publish attempt is logged to PUBLISH_LOG_PATH
// as "ms,screen,published". published = 1 means the bytes were written to
// the socket, not that the station received them; only an ack shows that.
#pragma once

// Sets the broker address, keepalive, timeouts and the ack callback. Does not connect yet.
void startMqtt();

// Call on every loop pass with the current screen. While EVA-LINK is up it
// keeps the MQTT connection (retrying every MQTT_RETRY_MS), receives the
// acks and publishes the screen every HEARTBEAT_MS.
void maintainMqtt(bool networkUp, const char* screen);

// Publishes the screen now (on a button press) and restarts the heartbeat timer.
void publishScreen(const char* screen);

// true while the MQTT session with the broker is open. Says nothing about
// whether the station is receiving: use mqttLive() for that.
bool mqttUp();

// true when the station is really receiving our data: the MQTT session is
// open AND the last ack arrived less than ACK_TIMEOUT_MS ago. Drives the
// link dot.
bool mqttLive();
