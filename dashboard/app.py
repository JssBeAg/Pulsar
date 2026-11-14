"""Dashboard de Pulsar (Streamlit).

Lee SQLite, NUNCA se conecta a MQTT: así se puede desarrollar sin el ESP32
y abrir varias pantallas no duplica datos.

Ejecutar desde la raíz del proyecto:

    streamlit run dashboard/app.py --server.address 0.0.0.0
"""

import sqlite3
import sys
import time
from contextlib import closing
from datetime import datetime
from pathlib import Path

import pandas as pd
import streamlit as st

# Reutilizamos config.py y db.py del backend.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "backend"))
import config  # noqa: E402
import db  # noqa: E402

INTERVALO_REFRESCO_S = 2
VENTANA_GRAFICA_S = 60 * 60       # Última hora
SEGUNDOS_SIN_DATOS_ALERTA = 10    # Si no llegan datos en este tiempo, se avisa

EMOJI_NIVEL = {"VERDE": "🟢", "AMARILLO": "🟡", "ROJO": "🔴"}
GRAVEDAD = {"VERDE": 0, "AMARILLO": 1, "ROJO": 2}
ORDEN_SISTEMAS = ["Cardiovascular", "Respiratorio", "Termico", "Ambiente"]

st.set_page_config(page_title="Pulsar", page_icon="🛰️", layout="wide")
db.crear_tablas()  # Por si el ingestor todavía no ha corrido


def consultar(sql, parametros=()):
    """Ejecuta una consulta de solo lectura y devuelve un DataFrame."""
    with closing(sqlite3.connect(db.RUTA_DB)) as con:
        return pd.read_sql_query(sql, con, params=parametros)


def ultimas_evaluaciones():
    """Última evaluación de cada sistema."""
    return consultar(
        """SELECT ts, sistema, nivel, motivo, accion FROM evaluaciones
           WHERE rowid IN (
               SELECT MAX(rowid) FROM evaluaciones WHERE astronauta = ? GROUP BY sistema
           )""",
        (config.ASTRONAUTA,),
    )


def lecturas_recientes(variables, desde_ts):
    marcadores = ",".join("?" for _ in variables)
    return consultar(
        f"""SELECT ts, variable, valor FROM lecturas
            WHERE astronauta = ? AND variable IN ({marcadores}) AND ts >= ?
            ORDER BY ts""",
        (config.ASTRONAUTA, *variables, desde_ts),
    )


def seccion_en_vivo():
    """Semáforo por sistema, acción recomendada y gráficas. Se refresca sola."""
    st.title("🛰️ Pulsar: estado del astronauta")

    evaluaciones = ultimas_evaluaciones()
    if evaluaciones.empty:
        st.info("Todavía no hay datos. ¿Está corriendo `python backend/ingestor.py`?")
        return

    # Aviso si dejaron de llegar datos (por ejemplo, se desenchufó el access point).
    segundos_sin_datos = int(time.time()) - int(evaluaciones["ts"].max())
    if segundos_sin_datos > SEGUNDOS_SIN_DATOS_ALERTA:
        st.warning(f"Sin datos nuevos desde hace {segundos_sin_datos} s. Posible pérdida de comunicación.")

    # Estado general = el sistema más grave
    evaluaciones["gravedad"] = evaluaciones["nivel"].map(GRAVEDAD)
    peor = evaluaciones.sort_values("gravedad", ascending=False).iloc[0]
    st.header(f"{EMOJI_NIVEL.get(peor['nivel'], '⚪')} Estado general: {peor['nivel']}")
    st.subheader(f"Acción recomendada: {peor['accion']}")
    st.caption(f"{peor['sistema']}: {peor['motivo']}")

    # Semáforo por sistema
    columnas = st.columns(len(ORDEN_SISTEMAS))
    for columna, sistema in zip(columnas, ORDEN_SISTEMAS):
        fila = evaluaciones[evaluaciones["sistema"] == sistema]
        with columna:
            if fila.empty:
                st.markdown(f"### ⚪ {sistema}")
                st.write("Sin datos")
                continue
            fila = fila.iloc[0]
            st.markdown(f"### {EMOJI_NIVEL.get(fila['nivel'], '⚪')} {sistema}")
            st.write(fila["motivo"])
            st.caption(fila["accion"])

    # Gráficas de la última hora
    lecturas = lecturas_recientes(["pulso", "spo2"], int(time.time()) - VENTANA_GRAFICA_S)
    if lecturas.empty:
        return
    lecturas["hora"] = pd.to_datetime(lecturas["ts"].map(datetime.fromtimestamp))
    tabla = lecturas.pivot_table(index="hora", columns="variable", values="valor")

    izquierda, derecha = st.columns(2)
    with izquierda:
        st.markdown("**Pulso (bpm), última hora**")
        if "pulso" in tabla:
            st.line_chart(tabla["pulso"])
    with derecha:
        st.markdown("**SpO₂ (%), última hora**")
        if "spo2" in tabla:
            st.line_chart(tabla["spo2"])


def seccion_checkin():
    """Check-in diario de salud conductual."""
    st.divider()
    st.header("🧠 Check-in diario")
    with st.form("checkin", clear_on_submit=True):
        animo = st.slider("Ánimo (1 = muy mal, 5 = muy bien)", 1, 5, 3)
        sueno = st.slider("Sueño (1 = muy mal, 5 = muy bien)", 1, 5, 3)
        estres = st.slider("Estrés (1 = nada, 5 = mucho)", 1, 5, 3)
        nota = st.text_area("Nota (opcional)")
        if st.form_submit_button("Guardar check-in"):
            db.guardar_checkin(int(time.time()), config.ASTRONAUTA, animo, sueno, estres, nota.strip())
            st.success("Check-in guardado.")
    # TODO: recomendaciones a partir de los check-ins (RF8), ej. 3 días con estrés alto y mal sueño.


# Refresco automático cada 2 s.
# Con st.fragment solo se vuelve a dibujar la parte en vivo, así el formulario
# de check-in no se interrumpe mientras se llena. Si la versión de Streamlit es
# vieja y no tiene st.fragment, se recarga toda la página con sleep + rerun.
if hasattr(st, "fragment"):
    st.fragment(run_every=INTERVALO_REFRESCO_S)(seccion_en_vivo)()
    seccion_checkin()
else:
    seccion_en_vivo()
    seccion_checkin()
    time.sleep(INTERVALO_REFRESCO_S)
    st.rerun()
