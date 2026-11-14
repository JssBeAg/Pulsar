# Pulsar 🛰️

**Monitoreo de salud para astronautas en misiones de larga duración que funciona aunque no haya comunicación con la Tierra.**

Proyecto para **NASA Space Apps Challenge 2026** (14 de noviembre de 2026).

---

## Índice

1. [El problema](#1-el-problema)
2. [Nuestra solución](#2-nuestra-solución)
3. [Arquitectura](#3-arquitectura)
4. [Por qué lo diseñamos así](#4-por-qué-lo-diseñamos-así)
5. [Herramientas](#5-herramientas)
6. [Requerimientos](#6-requerimientos)
7. [Estructura del repositorio](#7-estructura-del-repositorio)
8. [Cómo correrlo](#8-cómo-correrlo)
9. [Contrato de datos (MQTT)](#9-contrato-de-datos-mqtt)
10. [Reglas de salud](#10-reglas-de-salud)
11. [El demo](#11-el-demo)
12. [Estado y prioridades](#12-estado-y-prioridades)
13. [Cómo trabajamos con Git](#13-cómo-trabajamos-con-git)
14. [Glosario](#14-glosario)

---

## 1. El problema

<<<<<<< HEAD
En una misión larga (por ejemplo, a Marte) el cuerpo del astronauta cambia: el corazón, los huesos, el sistema inmune, el sueño y el estado de ánimo. Los riesgos principales son: holaholaaaaaaaa pousito 127
=======
En una misión larga (por ejemplo, a Marte) el cuerpo del astronauta presenta cambios constantes en distintas partes del cuerpo: el corazón, los huesos, el sistema inmune, el sueño y el estado de ánimo. Pero ademas de eso los riesgos principales son:
>>>>>>> 08cc586bf45668755f4f6f1c5456181f9b1d7b9c

- **Radiación**
- **Aislamiento y confinamiento**
- **Gravedad alterada**
- **Ambiente cerrado y hostil** (por ejemplo, CO₂ alto)

El problema es que **no hay un médico al lado** y la comunicación con la Tierra puede tardar minutos o cortarse. El astronauta necesita algo que le diga **"estás bien"** o **"algo anda mal, haz esto"**, sin depender de nadie más.

El reto pide un sistema que:
- recolecte indicadores de salud,
- permita al astronauta **evaluar y actuar** sobre su estado (no solo ver gráficas),
- funcione **offline y en local**,
- incluya la **salud conductual** (mente, no solo cuerpo).

## 2. Nuestra solución

Pulsar tiene dos partes:

1. **Un dispositivo que lleva el astronauta (ESP32).** Mide sus signos vitales, decide si están bien o mal, se lo muestra en una pantalla con un **semáforo** (verde, amarillo o rojo) y una **acción recomendada**, y guarda un registro. **Funciona solo, sin red.**
2. **Una estación de monitoreo (laptop).** Cuando hay comunicación, recibe los datos, guarda el historial, analiza tendencias de varios días, registra la salud conductual y permite que otras personas vean el estado del astronauta desde su navegador.

> **Idea central:** el dispositivo del astronauta **no depende** de la comunicación. La laptop es un extra, no una dependencia.

Por ahora los signos vitales se **simulan con potenciómetros** (perillas): al girarlas fingimos un pulso de 40 o de 170 cuando queremos para el demo. Más adelante se cambian por sensores reales sin tocar el resto del sistema.

## 3. Arquitectura

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

### Quién hace qué

| Pieza | En la vida real sería… | Su trabajo |
|---|---|---|
| **ESP32 sensor** | Un wearable o monitor personal | Medir, decidir y avisar al astronauta. **Siempre funciona.** |
| **ESP32 access point** | El router de la nave | **Solo mover paquetes.** Desenchufarlo simula perder la comunicación. |
| **Mosquitto** | El "cartero" de mensajes | Recibir mensajes y entregarlos a quien esté suscrito. |
| **ingestor.py** | Servicio de la computadora de a bordo | Recibir los datos, guardarlos y aplicar reglas de largo plazo. |
| **SQLite** | Base de datos de la misión | Guardar el historial. |
| **Streamlit** | Pantallas de monitoreo | Mostrar semáforo, gráficas, acciones y el check-in de ánimo. |

### El sistema contado en versiones

Cada pieza existe porque resuelve un problema de la anterior:

1. **Medir:** un potenciómetro + ESP32 nos da un número. *Problema: nadie lo ve.*
2. **Mostrar y decidir en el dispositivo:** pantalla OLED + reglas. El astronauta ya sabe si está bien. *Problema: no hay historial ni nadie más lo ve.*
3. **Enviar:** por WiFi a la laptop. *Problema: ¿en qué formato? ¿y si varios programas quieren los datos?*
4. **Un cartero (MQTT):** el ESP32 **publica** en un "buzón" y cualquier programa **suscrito** recibe una copia. *Problema: los mensajes se pierden.*
5. **Guardar:** un programa de Python guarda todo en SQLite. *Problema: un número solo no dice si está bien.*
6. **Reglas:** el número se convierte en verde, amarillo o rojo + una acción.
7. **Línea base personal:** el sistema aprende lo "normal" de cada astronauta y detecta cambios que en otra persona serían normales.
8. **Dashboard:** Streamlit muestra todo a los monitores.
9. **Salud conductual:** un check-in diario de ánimo, sueño y estrés.

## 4. Por qué lo diseñamos así

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

## 5. Herramientas

### Hardware
- 2 × **ESP32** (DevKit clásico): uno sensor y uno access point
- **Potenciómetros B10K** lineales: simulan los sensores
- **Pantalla OLED SSD1306** 128×64 (I2C)
- Protoboard, cables y cable USB **de datos**
- Laptop (Windows)

### Software
| Herramienta | Para qué |
|---|---|
| **VS Code** | Editor de todo el proyecto |
| **PlatformIO** (extensión de VS Code) | Compilar y subir el firmware al ESP32 |
| **Arduino framework** (C++) | Lenguaje del firmware |
| **Docker / Docker Compose** | Correr Mosquitto sin instalarlo a mano |
| **Mosquitto** (`eclipse-mosquitto:2`) | Broker MQTT |
| **Python 3** | Backend y dashboard |
| **paho-mqtt** | Que Python se conecte a MQTT |
| **SQLite** | Base de datos (viene incluida con Python) |
| **Streamlit** | Dashboard web |
| **pandas** | Manejar los datos para las gráficas |
| **MQTT Explorer** | Ver los mensajes MQTT en vivo (depuración) |
| **Git + GitHub** | Control de versiones |

### Librerías del firmware (PlatformIO)
- `knolleary/PubSubClient`: cliente MQTT
- `adafruit/Adafruit SSD1306` y `adafruit/Adafruit GFX Library`: pantalla OLED
- `LittleFS` y `Preferences`: incluidas en el core del ESP32

## 6. Requerimientos

### Requerimientos funcionales
- **RF1.** El dispositivo lee pulso, SpO₂, temperatura, CO₂ y radiación (simulados).
- **RF2.** El dispositivo clasifica cada sistema en verde, amarillo o rojo y muestra una acción recomendada en la pantalla.
- **RF3.** El dispositivo aprende una línea base personal y avisa cuando un valor se aleja de lo normal *para esa persona*.
- **RF4.** El dispositivo guarda un registro local aunque no haya red.
- **RF5.** Cuando hay red, el dispositivo envía los datos por MQTT a la estación.
- **RF6.** La estación guarda el historial en una base de datos.
- **RF7.** El dashboard muestra el estado de cada sistema, gráficas y acciones recomendadas.
- **RF8.** El astronauta puede registrar un check-in diario de ánimo, sueño y estrés, y el sistema genera recomendaciones a partir de él.
- **RF9.** Varias computadoras pueden ver el dashboard al mismo tiempo.

### Requerimientos no funcionales
- **RNF1. Autonomía:** el dispositivo funciona sin WiFi, sin laptop y sin internet.
- **RNF2. Offline:** nada del sistema necesita internet.
- **RNF3. No bloqueo:** perder la red nunca congela la pantalla ni detiene el registro.
- **RNF4. Reemplazable:** cambiar un potenciómetro por un sensor real solo cambia la función de lectura.
- **RNF5. Simple:** se puede levantar en una laptop con Docker y Python.

### Fuera de alcance (por ahora)
- Sensores reales (MAX30102, MPU6050, MLX90614/DS18B20, MQ-135/SCD40)
- Tarjeta SD
- Varios astronautas
- Reenvío automático de lo guardado mientras no hubo red (se hace si sobra tiempo)

## 7. Estructura del repositorio

```
pulsar/
├── firmware/
│   ├── sensor/            # ESP32 del astronauta (PlatformIO)
│   └── access-point/      # ESP32 que hace de red de la nave (PlatformIO)
├── backend/               # ingestor.py, reglas, SQLite
├── dashboard/             # app de Streamlit
├── broker/                # mosquitto.conf
├── docs/                  # arquitectura, diagramas, pitch
├── docker-compose.yml
├── requirements.txt
├── .gitignore
└── README.md
```

`.gitignore`:
```
.venv/
__pycache__/
.pio/
*.db
```

## 8. Cómo correrlo

> Algunas carpetas todavía están en construcción. Esta sección se actualiza conforme cada parte funcione.

### 8.1 Preparar Python (con internet, antes de conectarse al access point)
```powershell
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```
`requirements.txt`:
```
paho-mqtt
streamlit
pandas
```

### 8.2 Levantar el broker
`broker/mosquitto.conf`:
```
listener 1883 0.0.0.0
allow_anonymous true
```
`docker-compose.yml`:
```yaml
services:
  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "1883:1883"
    volumes:
      - ./broker/mosquitto.conf:/mosquitto/config/mosquitto.conf
```
```powershell
docker compose up -d
```
Comprueba con **MQTT Explorer** (host `localhost`, puerto `1883`).

### 8.3 Red de la nave (ESP32 access point)
1. Sube el firmware de `firmware/access-point/`.
2. Conecta la laptop a esa red WiFi.
3. Ponle a la laptop una **IP fija**: `192.168.4.10` (máscara `255.255.255.0`, puerta de enlace `192.168.4.1`).
4. En el **Firewall de Windows**, abre los puertos TCP **1883** (MQTT) y **8501** (Streamlit) para el perfil **Público**.

> ⚠️ Conectada al access point, la laptop **no tiene internet**. Para hacer push a GitHub cambia de red o usa cable.

### 8.4 ESP32 sensor
`firmware/sensor/platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
lib_deps =
    knolleary/PubSubClient
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
```
Conexiones:

| Componente | Pin del ESP32 |
|---|---|
| Potenciómetros (pata central) | GPIO 32, 33, 34, 35, 36, 39 (**solo ADC1**) |
| Potenciómetros (extremos) | 3.3V y GND |
| OLED SDA / SCL | GPIO 21 / GPIO 22 |

> Usamos solo pines ADC1 porque **ADC2 no funciona con el WiFi encendido**.

Sube el firmware y abre el monitor serial (115200).

### 8.5 Backend y dashboard
```powershell
python backend/ingestor.py
streamlit run dashboard/app.py --server.address 0.0.0.0
```
- En la laptop: `http://localhost:8501`
- Desde otra PC en la misma red: `http://192.168.4.10:8501`

## 9. Contrato de datos (MQTT)

Este es el "idioma" acordado entre el firmware y el backend. **Si lo cambias, avisa al equipo.**

| Tópico | Quién publica | Contenido |
|---|---|---|
| `pulsar/astro1/vitales` | ESP32 sensor | `{"pulso": 72, "spo2": 98, "temp": 36.6}` |
| `pulsar/astro1/ambiente` | ESP32 sensor | `{"co2": 850, "rad": 0.8}` |
| `pulsar/astro1/estado` | Laptop (opcional) | Estado calculado |

- 1 mensaje por segundo.
- La hora (`ts`) la pone el ingestor al recibir el mensaje, así el ESP32 no necesita reloj.

### Base de datos (SQLite)
| Tabla | Columnas |
|---|---|
| `lecturas` | ts, astronauta, variable, valor |
| `evaluaciones` | ts, astronauta, sistema, nivel, motivo, accion |
| `checkins` | ts, astronauta, animo, sueno, estres, nota |

`lecturas` usa una fila por variable: si se agrega un sensor nuevo, no hay que cambiar la tabla.

## 10. Reglas de salud

> ⚠️ Valores **de demostración**, no son criterios médicos.

| Sistema | Variable | Verde | Amarillo | Rojo |
|---|---|---|---|---|
| Cardiovascular | Pulso (bpm) | 50–100 | 40–50 / 100–130 | <40 / >130 |
| Respiratorio | SpO₂ (%) | ≥95 | 90–94 | <90 |
| Térmico | Temperatura (°C) | 36–37.5 | 35.5–36 / 37.5–38.5 | <35.5 / >38.5 |
| Ambiente | CO₂ (ppm) | <1000 | 1000–2500 | >2500 |
| Radiación | Dosis acumulada | Comparada contra un límite diario | | |

Rangos que simula cada potenciómetro: pulso 40–180 bpm, SpO₂ 85–100 %, temperatura 35–40 °C, CO₂ 400–5000 ppm, radiación simulada.

**Cada regla devuelve tres cosas:** nivel, motivo y acción. Ejemplo:
> 🟡 *SpO₂ 91 %, debajo de tu normal (97 %).* → Revisa el sensor, reduce la actividad y verifica el soporte vital de la cabina.

**Línea base personal:** los primeros minutos el sistema calcula el promedio y la variación de cada astronauta. Si un valor se aleja mucho de *su* normal (más de 2 desviaciones), pasa a amarillo aunque esté dentro del rango general. Se guarda en la memoria NVS para no perderse si el ESP32 se reinicia.

**Salud conductual:** check-in diario (ánimo, sueño y estrés, del 1 al 5). Ejemplo de regla:
> 3 días seguidos con estrés alto y mal sueño → contacto con el psicólogo de la misión en la próxima ventana de comunicación + rutina de descompresión.

## 11. El demo

1. Todo funcionando: el OLED y el dashboard muestran los mismos datos.
2. Giramos una perilla → el pulso sube → el OLED y el dashboard pasan a rojo con su acción.
3. **Desenchufamos el ESP32 access point** ("se perdió la comunicación con la nave").
4. El OLED **sigue funcionando** y el registro sigue guardándose. El astronauta no se queda sin información.
5. Reconectamos → la laptop vuelve a recibir datos.

### Siguientes pasos (para el pitch)
- En una misión real el broker viviría en **hardware dedicado y redundante**, no en una laptop.
- Para nave ↔ Tierra: un broker de cada lado unidos con **Mosquitto bridge**. Cada lado funciona solo y se sincronizan cuando hay enlace.
- Sensores reales, tarjeta SD, varios astronautas e interfaz más pulida.

## 12. Estado y prioridades

En orden:

- [ ] **1.** ESP32: lectura + reglas + OLED (el núcleo autónomo)
- [ ] **2.** ESP32 access point
- [ ] **3.** Envío MQTT → Mosquitto → `ingestor.py` → SQLite
- [ ] **4.** Dashboard en Streamlit (semáforo + acciones + gráficas)
- [ ] **5.** Check-in de salud conductual
- [ ] **6.** Registro CSV en LittleFS
- [ ] **7.** Línea base personal
- [ ] **8.** *(Si sobra tiempo)* Reenviar lo guardado al reconectar
- [ ] README final, video y presentación

### Reparto por área
| Área | Rama | Qué incluye |
|---|---|---|
| Firmware | `firmware` | ESP32 sensor y access point |
| Backend | `backend` | Ingestor, reglas y SQLite |
| Dashboard | `dashboard` | Streamlit y check-in |
| Docs / pitch | `docs` | README, diagramas, video y presentación |

## 13. Cómo trabajamos con Git

- Una rama por área (ver tabla de arriba). `main` siempre debe funcionar.
- Antes de empezar: **pull**. Al terminar algo que funcione: **commit + push**.
- Commits pequeños con mensajes claros, por ejemplo `firmware: lectura de pulso y semáforo en OLED`.
- Si cambias el **contrato MQTT** o las **tablas**, avisa al equipo: rompe a los demás.

## 14. Glosario

| Término | Qué significa |
|---|---|
| **ESP32** | Microcontrolador barato con WiFi. Es el "cerebro" del dispositivo. |
| **Potenciómetro** | Perilla que cambia un voltaje. El ESP32 lo lee y lo tratamos como si fuera un signo vital. |
| **ADC** | Convertidor analógico-digital: transforma un voltaje (0–3.3V) en un número (0–4095). |
| **OLED** | Pantallita pequeña donde el astronauta ve su estado. |
| **I2C / SPI** | Formas de conectar componentes al ESP32 con pocos cables. |
| **Access point** | Dispositivo que crea una red WiFi para que otros se conecten. |
| **MQTT** | Protocolo de mensajes ligero, muy usado en IoT. Funciona con "buzones" (tópicos). |
| **Broker** | El "cartero" de MQTT: recibe mensajes y los entrega a quien esté suscrito. Nosotros usamos Mosquitto. |
| **Publicar / suscribirse** | Mandar un mensaje a un tópico / pedir recibir los mensajes de un tópico. |
| **Tópico** | El nombre del "buzón", por ejemplo `pulsar/astro1/vitales`. |
| **Edge computing** | Procesar los datos en el dispositivo mismo, no en un servidor lejano. |
| **Store-and-forward** | Guardar los datos cuando no hay red y enviarlos cuando vuelve. |
| **LittleFS** | Sistema de archivos que usa la memoria interna del ESP32 como si fuera un disco pequeño. |
| **NVS / Preferences** | Memoria del ESP32 para guardar pocos datos que no se borran al reiniciar. |
| **Línea base** | Lo "normal" de una persona específica. |
| **SQLite** | Base de datos que es solo un archivo; no necesita servidor. |
| **Streamlit** | Librería de Python para hacer páginas web con gráficas rápidamente. |
| **Docker** | Herramienta para correr programas en "contenedores" sin instalarlos directamente. |

---

Hecho por el equipo Pulsar para NASA Space Apps Challenge 2026.