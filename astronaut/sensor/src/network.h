// network.h — WiFi and MQTT. The network is OPTIONAL: nothing here blocks the program.
#pragma once
#include "readings.h"

// Starts WiFi in station mode. Does not wait for the connection.
void initNetwork();

// Call on every loop() pass. If WiFi or MQTT was lost, it retries every
// 5 seconds without using while or delay. It also services the MQTT client.
void maintainNetwork();

// true if connected to the MQTT broker.
bool mqttConnected();

// Publishes the readings to pulsar/<ASTRONAUT_ID>/vitals and pulsar/<ASTRONAUT_ID>/environment.
// Without a connection it does nothing (does not block). The JSON has NO ts: the ingestor adds it.
void publishVitals(const Readings& r);
