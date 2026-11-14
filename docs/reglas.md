# Reglas de salud (fuente de verdad)

> ⚠️ Valores **de demostración**, no son criterios médicos.

Este archivo es la **fuente de verdad** de los umbrales. Si cambias algo aquí, cámbialo también en:

- `firmware/sensor/src/reglas.cpp`
- `backend/reglas.py`

## Umbrales

| Sistema | Variable | Verde | Amarillo | Rojo |
|---|---|---|---|---|
| Cardiovascular | Pulso (bpm) | 50–100 | 40–50 / 100–130 | <40 / >130 |
| Respiratorio | SpO₂ (%) | ≥95 | 90–94 | <90 |
| Termico | Temperatura (°C) | 36–37.5 | 35.5–36 / 37.5–38.5 | <35.5 / >38.5 |
| Ambiente | CO₂ (ppm) | <1000 | 1000–2500 | >2500 |

### Valores exactamente en el límite

Así resuelve el código los valores que caen justo en un borde:

- **Pulso:** 40 → amarillo, 50 → verde, 100 → verde, 130 → amarillo.
- **SpO₂:** 90 → amarillo, 95 → verde.
- **Temperatura:** 35.5 → amarillo, 36 → verde, 37.5 → verde, 38.5 → amarillo.
- **CO₂:** 1000 → amarillo, 2500 → amarillo.

## Qué devuelve cada regla

Cada regla devuelve **nivel, motivo y acción**. Los textos no llevan acentos ni ñ y miden como máximo 21 caracteres, para que quepan en una línea de la OLED.

| Variable | Nivel | Motivo | Acción |
|---|---|---|---|
| Pulso | Rojo (bajo) | Pulso muy bajo | Reposo y aviso medico |
| Pulso | Amarillo (bajo) | Pulso bajo | Revisa sensor, reposa |
| Pulso | Verde | Pulso normal | Continua normal |
| Pulso | Amarillo (alto) | Pulso alto | Baja ritmo y respira |
| Pulso | Rojo (alto) | Pulso muy alto | Detente y pide ayuda |
| SpO₂ | Rojo | SpO2 muy baja | Oxigeno y pide ayuda |
| SpO₂ | Amarillo | SpO2 baja | Revisa soporte vital |
| SpO₂ | Verde | SpO2 normal | Continua normal |
| Temperatura | Rojo (baja) | Temp muy baja | Abrigate y avisa |
| Temperatura | Amarillo (baja) | Temp baja | Abrigate |
| Temperatura | Verde | Temp normal | Continua normal |
| Temperatura | Amarillo (alta) | Temp alta | Hidratate y reposa |
| Temperatura | Rojo (alta) | Fiebre alta | Pide ayuda medica |
| CO₂ | Verde | CO2 normal | Continua normal |
| CO₂ | Amarillo | CO2 elevado | Revisa depurador CO2 |
| CO₂ | Rojo | CO2 peligroso | Mascarilla y alerta |

**Estado general:** el peor nivel de todos los sistemas. Si hay empate, la OLED muestra el primero en este orden: pulso, SpO₂, temperatura, CO₂.

## Pendiente

- **Radiación:** dosis acumulada comparada contra un límite diario. *Límite por definir.*
- **Línea base personal:** más de 2 desviaciones de *su* normal → amarillo (README, sección 10).
- **Salud conductual:** reglas a partir del check-in (ej. 3 días seguidos con estrés alto y mal sueño).
