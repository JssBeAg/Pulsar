# Arquitectura de Pulsar

Resumen de una página. El detalle completo está en el [README](../README.md).

## Idea central

El dispositivo del astronauta **funciona solo**. La red y la laptop son un extra, no una dependencia.

```
               ┌──────────── ESP32 sensor (lo lleva el astronauta) ──────────┐
potenciómetros→│ leer → reglas + línea base → pantalla OLED (valor + semáforo)│
               │                 ↓                                           │
               │        registro CSV en la memoria interna                   │
               └─────────────────┬───────────────────────────────────────────┘
                                 │ WiFi (solo si hay)
                     [ESP32 #2 = access point, "la red de la nave"]
                                 │
  Laptop ("computadora de a bordo"):
     Mosquitto (Docker) → ingestor.py → SQLite → Streamlit (dashboard)
                                                      ↑
                                 otras PCs lo ven desde el navegador
```

## Quién hace qué

| Pieza | Código | Su trabajo |
|---|---|---|
| ESP32 sensor | `firmware/sensor/` | Medir, decidir y avisar al astronauta. Siempre funciona. |
| ESP32 access point | `firmware/access-point/` | Solo mover paquetes. Desenchufarlo simula perder la comunicación. |
| Mosquitto | `broker/`, `docker-compose.yml` | Recibir mensajes MQTT y entregarlos a quien esté suscrito. |
| Ingestor | `backend/ingestor.py` | Único programa que escribe en SQLite: guarda lecturas y evaluaciones. |
| Dashboard | `dashboard/app.py` | Lee SQLite y muestra semáforo, acciones, gráficas y check-in. |

### Módulos del firmware sensor

| Archivo | Responsabilidad |
|---|---|
| `lectura.*` | Leer las variables. **Único** archivo a cambiar al usar sensores reales. |
| `reglas.*` | Número → semáforo + motivo + acción. |
| `pantalla.*` | OLED: estado general, acción y gráfica del pulso. |
| `registro.*` | CSV en LittleFS, escrito en bloque cada 30 s. |
| `red.*` | WiFi + MQTT sin bloqueo (reintenta cada 5 s). |
| `main.cpp` | Orquesta todo con `millis()`, sin `delay`. |

## Decisiones y por qué

| Decisión | Por qué |
|---|---|
| **El ESP32 decide solo (edge computing)** | El reto pide no depender de la Tierra. Si se cae la red, el astronauta sigue viendo su estado. |
| **Guardar en la memoria interna del ESP32 (LittleFS), no en tarjeta SD** | No tenemos lector SD. La flash interna tiene ~1.4 MB libres: unas 13 horas guardando 1 lectura por segundo, o ~5 días con 1 cada 10 segundos. Suficiente para el demo y sin hardware extra. |
| **CSV en el ESP32, no SQLite** | SQLite en un ESP32 es posible pero pesado y fácil de romper. Un CSV es simple y se abre en Excel. |
| **Escribir en la flash cada 30–60 s, no en cada lectura** | La memoria flash se desgasta con muchas escrituras pequeñas. Juntamos líneas en RAM y las escribimos de golpe. |
| **Un segundo ESP32 como access point** | El WiFi del evento es malo. Además, desenchufarlo frente a los jueces es una forma muy visual de simular la pérdida de comunicación. |
| **Mosquitto en la laptop, no en el access point** | El router solo debe mover paquetes; los servicios van en la "computadora de a bordo". En una misión real esto viviría en hardware dedicado y redundante (por ejemplo, una Raspberry Pi como *gateway*). Hoy la laptop hace de servidor y de pantalla a la vez. |
| **Streamlit lee SQLite, no MQTT** | Solo un programa (`ingestor.py`) escribe en la base. Así el dashboard se puede desarrollar sin el ESP32 y abrir varias pantallas no duplica datos. |
| **Python + Streamlit (no Next.js)** | Todo en un solo lenguaje y un dashboard en horas, no en todo el día. Varias PCs pueden verlo desde el navegador. Next.js queda como mejora después del hackathon. |
| **La reconexión WiFi no bloquea el programa** | Si el ESP32 se queda "esperando" la red, la pantalla se congela y perdemos justo lo que queremos demostrar. Reintenta cada pocos segundos sin detener lo demás. |

## Flujo de datos

1. Cada 1 s el sensor lee, evalúa y muestra el resultado en la OLED, y agrega una línea al registro en RAM.
2. Si hay red, publica `pulsar/astro1/vitales` y `pulsar/astro1/ambiente` (JSON sin `ts`).
3. Cada 30 s vuelca el registro a `/registro.csv`.
4. El ingestor recibe cada mensaje, le pone `ts`, guarda en `lecturas`, evalúa y guarda en `evaluaciones`.
5. El dashboard lee SQLite cada 2 s.
