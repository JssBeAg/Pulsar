"""Reglas de salud de Pulsar.

Fuente de verdad: docs/reglas.md; mantener sincronizado con firmware/sensor/src/reglas.cpp

Cada función devuelve (nivel, motivo, accion), con nivel = "VERDE" | "AMARILLO" | "ROJO".
Los textos son los mismos que en el firmware (sin acentos, porque la OLED no
los dibuja) para que el dashboard y la pantalla digan exactamente lo mismo.

⚠️ Valores de demostración, no son criterios médicos.
"""

VERDE = "VERDE"
AMARILLO = "AMARILLO"
ROJO = "ROJO"

# ===== Umbrales del pulso (bpm) =====
PULSO_ROJO_BAJO = 40.0   # < 40 → rojo
PULSO_VERDE_MIN = 50.0   # 40–50 → amarillo
PULSO_VERDE_MAX = 100.0  # 50–100 → verde
PULSO_ROJO_ALTO = 130.0  # 100–130 → amarillo, > 130 → rojo

# ===== Umbrales de SpO2 (%) =====
SPO2_ROJO_BAJO = 90.0  # < 90 → rojo
SPO2_VERDE_MIN = 95.0  # 90–94 → amarillo, ≥ 95 → verde

# ===== Umbrales de temperatura (°C) =====
TEMP_ROJO_BAJO = 35.5  # < 35.5 → rojo
TEMP_VERDE_MIN = 36.0  # 35.5–36 → amarillo
TEMP_VERDE_MAX = 37.5  # 36–37.5 → verde
TEMP_ROJO_ALTO = 38.5  # 37.5–38.5 → amarillo, > 38.5 → rojo

# ===== Umbrales de CO2 (ppm) =====
CO2_VERDE_MAX = 1000.0  # < 1000 → verde
CO2_ROJO_ALTO = 2500.0  # 1000–2500 → amarillo, > 2500 → rojo


def evaluar_pulso(bpm):
    if bpm < PULSO_ROJO_BAJO:
        return ROJO, "Pulso muy bajo", "Reposo y aviso medico"
    if bpm < PULSO_VERDE_MIN:
        return AMARILLO, "Pulso bajo", "Revisa sensor, reposa"
    if bpm <= PULSO_VERDE_MAX:
        return VERDE, "Pulso normal", "Continua normal"
    if bpm <= PULSO_ROJO_ALTO:
        return AMARILLO, "Pulso alto", "Baja ritmo y respira"
    return ROJO, "Pulso muy alto", "Detente y pide ayuda"


def evaluar_spo2(porcentaje):
    if porcentaje < SPO2_ROJO_BAJO:
        return ROJO, "SpO2 muy baja", "Oxigeno y pide ayuda"
    if porcentaje < SPO2_VERDE_MIN:
        return AMARILLO, "SpO2 baja", "Revisa soporte vital"
    return VERDE, "SpO2 normal", "Continua normal"


def evaluar_temp(grados):
    if grados < TEMP_ROJO_BAJO:
        return ROJO, "Temp muy baja", "Abrigate y avisa"
    if grados < TEMP_VERDE_MIN:
        return AMARILLO, "Temp baja", "Abrigate"
    if grados <= TEMP_VERDE_MAX:
        return VERDE, "Temp normal", "Continua normal"
    if grados <= TEMP_ROJO_ALTO:
        return AMARILLO, "Temp alta", "Hidratate y reposa"
    return ROJO, "Fiebre alta", "Pide ayuda medica"


def evaluar_co2(ppm):
    if ppm < CO2_VERDE_MAX:
        return VERDE, "CO2 normal", "Continua normal"
    if ppm <= CO2_ROJO_ALTO:
        return AMARILLO, "CO2 elevado", "Revisa depurador CO2"
    return ROJO, "CO2 peligroso", "Mascarilla y alerta"


# TODO: regla de radiación (dosis acumulada vs. límite diario); el límite no está definido.
# TODO: reglas de largo plazo y de check-in (RF8, ej. 3 días de estrés alto + mal sueño).
