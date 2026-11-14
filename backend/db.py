"""Acceso a la base de datos SQLite de Pulsar.

Tablas (README, sección 9):
- lecturas:     ts, astronauta, variable, valor   (una fila por variable)
- evaluaciones: ts, astronauta, sistema, nivel, motivo, accion
- checkins:     ts, astronauta, animo, sueno, estres, nota

Solo el ingestor escribe lecturas y evaluaciones; el dashboard solo lee
(y escribe check-ins).
"""

import sqlite3
from contextlib import closing
from pathlib import Path

import config

# La ruta se resuelve desde la raíz del proyecto, para que funcione sin
# importar desde qué carpeta se ejecute el programa.
RAIZ_PROYECTO = Path(__file__).resolve().parent.parent
RUTA_DB = RAIZ_PROYECTO / config.DB_PATH


def _conectar():
    """Abre una conexión nueva (crea la carpeta data/ si no existe)."""
    RUTA_DB.parent.mkdir(parents=True, exist_ok=True)
    return sqlite3.connect(RUTA_DB)


def crear_tablas():
    """Crea las tablas si todavía no existen."""
    # closing() cierra la conexión; "with con" hace commit (o rollback si falla).
    with closing(_conectar()) as con, con:
        con.execute(
            """CREATE TABLE IF NOT EXISTS lecturas (
                   ts INTEGER NOT NULL,
                   astronauta TEXT NOT NULL,
                   variable TEXT NOT NULL,
                   valor REAL NOT NULL
               )"""
        )
        con.execute(
            """CREATE TABLE IF NOT EXISTS evaluaciones (
                   ts INTEGER NOT NULL,
                   astronauta TEXT NOT NULL,
                   sistema TEXT NOT NULL,
                   nivel TEXT NOT NULL,
                   motivo TEXT,
                   accion TEXT
               )"""
        )
        con.execute(
            """CREATE TABLE IF NOT EXISTS checkins (
                   ts INTEGER NOT NULL,
                   astronauta TEXT NOT NULL,
                   animo INTEGER NOT NULL,
                   sueno INTEGER NOT NULL,
                   estres INTEGER NOT NULL,
                   nota TEXT
               )"""
        )


def guardar_lectura(ts, astronauta, variable, valor):
    """Guarda el valor de una variable (pulso, spo2, temp, co2, rad...)."""
    with closing(_conectar()) as con, con:
        con.execute(
            "INSERT INTO lecturas (ts, astronauta, variable, valor) VALUES (?, ?, ?, ?)",
            (ts, astronauta, variable, valor),
        )


def guardar_evaluacion(ts, astronauta, sistema, nivel, motivo, accion):
    """Guarda el resultado de aplicar una regla (semáforo + motivo + acción)."""
    with closing(_conectar()) as con, con:
        con.execute(
            "INSERT INTO evaluaciones (ts, astronauta, sistema, nivel, motivo, accion) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (ts, astronauta, sistema, nivel, motivo, accion),
        )


def guardar_checkin(ts, astronauta, animo, sueno, estres, nota):
    """Guarda el check-in diario de salud conductual (valores del 1 al 5)."""
    with closing(_conectar()) as con, con:
        con.execute(
            "INSERT INTO checkins (ts, astronauta, animo, sueno, estres, nota) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (ts, astronauta, animo, sueno, estres, nota),
        )
