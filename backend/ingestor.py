"""Ingestor de Pulsar: MQTT → SQLite.

Es el ÚNICO programa que escribe lecturas y evaluaciones en la base.
Ejecutar desde la raíz del proyecto:

    python backend/ingestor.py
"""

import json
import time

import paho.mqtt.client as mqtt

import config
import db
import reglas

# Qué variables se evalúan, a qué sistema pertenecen y con qué regla.
# "rad" se guarda pero todavía no se evalúa (ver TODO en reglas.py).
EVALUADORES = {
    "pulso": ("Cardiovascular", reglas.evaluar_pulso),
    "spo2": ("Respiratorio", reglas.evaluar_spo2),
    "temp": ("Termico", reglas.evaluar_temp),
    "co2": ("Ambiente", reglas.evaluar_co2),
}


def al_conectar(cliente, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"[ingestor] Conexión rechazada por el broker: {reason_code}")
        return
    print(f"[ingestor] Conectado a {config.MQTT_HOST}:{config.MQTT_PORT}")
    # Suscribirse aquí hace que, al reconectar, se vuelva a suscribir solo.
    for topico in config.TOPICOS:
        cliente.subscribe(topico)
        print(f"[ingestor] Suscrito a {topico}")


def al_desconectar(cliente, userdata, flags, reason_code, properties):
    print(f"[ingestor] Desconectado ({reason_code}). Reintentando...")


def al_recibir(cliente, userdata, mensaje):
    # La hora la pone el ingestor: el ESP32 no tiene reloj.
    ts = int(time.time())

    try:
        datos = json.loads(mensaje.payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        print(f"[ingestor] Mensaje inválido en {mensaje.topic}: {mensaje.payload!r}")
        return
    if not isinstance(datos, dict):
        print(f"[ingestor] Se esperaba un objeto JSON en {mensaje.topic}")
        return

    for variable, valor in datos.items():
        try:
            valor = float(valor)
        except (TypeError, ValueError):
            print(f"[ingestor] Valor no numérico: {variable}={valor!r}")
            continue

        db.guardar_lectura(ts, config.ASTRONAUTA, variable, valor)

        if variable in EVALUADORES:
            sistema, evaluar = EVALUADORES[variable]
            nivel, motivo, accion = evaluar(valor)
            db.guardar_evaluacion(ts, config.ASTRONAUTA, sistema, nivel, motivo, accion)


def main():
    db.crear_tablas()

    cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pulsar-ingestor")
    cliente.on_connect = al_conectar
    cliente.on_disconnect = al_desconectar
    cliente.on_message = al_recibir
    cliente.reconnect_delay_set(min_delay=1, max_delay=10)

    # connect_async no falla si el broker todavía no está arriba;
    # loop_forever(retry_first_connection=True) sigue intentando hasta conectar.
    cliente.connect_async(config.MQTT_HOST, config.MQTT_PORT)
    print(f"[ingestor] Guardando en {db.RUTA_DB}")
    try:
        cliente.loop_forever(retry_first_connection=True)
    except KeyboardInterrupt:
        print("\n[ingestor] Detenido.")
        cliente.disconnect()


if __name__ == "__main__":
    main()
