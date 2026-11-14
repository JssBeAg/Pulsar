// red.h — WiFi y MQTT. La red es OPCIONAL: nada de aquí bloquea el programa.
#pragma once
#include "lectura.h"

// Arranca el WiFi en modo estación. No espera a conectarse.
void iniciarRed();

// Llamar en cada vuelta del loop(). Si se perdió el WiFi o MQTT, reintenta
// cada 5 segundos sin usar while ni delay. También atiende al cliente MQTT.
void mantenerRed();

// true si hay conexión con el broker MQTT.
bool mqttConectado();

// Publica las lecturas en pulsar/astro1/vitales y pulsar/astro1/ambiente.
// Sin conexión no hace nada (no bloquea). El JSON NO lleva ts: lo pone el ingestor.
void publicarVitales(const Lecturas& l);
