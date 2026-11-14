// pantalla.cpp — Implementación de la pantalla OLED.
//
// Distribución de la pantalla (128 x 64 píxeles):
//   y 0–15  : nivel general en grande ("VERDE", "AMARILLO", "ROJO")
//   y 18–25 : acción recomendada (una línea)
//   y 28–63 : a la izquierda el pulso actual, a la derecha la gráfica
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pantalla.h"
#include "config.h"

// ===== Hardware =====
static const int ANCHO_OLED = 128;
static const int ALTO_OLED = 64;
static const uint8_t DIRECCION_OLED = 0x3C;
static const int8_t SIN_PIN_RESET = -1;  // El módulo comparte el reset del ESP32

static Adafruit_SSD1306 oled(ANCHO_OLED, ALTO_OLED, &Wire, SIN_PIN_RESET);
static bool pantallaLista = false;

// ===== Buffer circular con los últimos valores de pulso =====
static const int TAM_HISTORIAL = 100;
static float historialPulso[TAM_HISTORIAL];
static int indiceSiguiente = 0;     // Posición donde se escribe el próximo valor
static int cantidadGuardada = 0;    // Cuántos valores válidos hay (máximo TAM_HISTORIAL)

// ===== Zona de la gráfica =====
static const int GRAFICA_X = 28;     // 128 - 28 = 100 píxeles: uno por valor
static const int GRAFICA_Y = 28;
static const int GRAFICA_ALTO = 36;  // 28 + 36 = 64
static const float GRAFICA_MIN = 40.0f;   // Mismo rango que simula el potenciómetro
static const float GRAFICA_MAX = 180.0f;

// Texto del nivel para mostrar en grande.
static const char* textoNivel(Nivel nivel) {
  switch (nivel) {
    case VERDE:    return "VERDE";
    case AMARILLO: return "AMARILLO";
    case ROJO:     return "ROJO";
  }
  return "?";
}

// Guarda un valor en el buffer circular (el más viejo se sobrescribe).
static void guardarEnHistorial(float pulso) {
  historialPulso[indiceSiguiente] = pulso;
  indiceSiguiente = (indiceSiguiente + 1) % TAM_HISTORIAL;
  if (cantidadGuardada < TAM_HISTORIAL) cantidadGuardada++;
}

// Convierte un valor de pulso a la coordenada y dentro de la gráfica.
static int valorAY(float valor) {
  float proporcion = (valor - GRAFICA_MIN) / (GRAFICA_MAX - GRAFICA_MIN);
  if (proporcion < 0) proporcion = 0;
  if (proporcion > 1) proporcion = 1;
  // En la pantalla la y crece hacia abajo, por eso se invierte.
  return GRAFICA_Y + (GRAFICA_ALTO - 1) - (int)(proporcion * (GRAFICA_ALTO - 1));
}

void iniciarPantalla() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  pantallaLista = oled.begin(SSD1306_SWITCHCAPVCC, DIRECCION_OLED);
  if (!pantallaLista) {
    Serial.println("[pantalla] No se encontro la OLED. El sistema sigue sin pantalla.");
    return;
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);  // La acción ocupa una sola línea; lo que sobre se corta
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.print("PULSAR");
  oled.display();
}

void mostrarEstado(const Evaluacion& ev, const Lecturas& l) {
  // El historial se guarda aunque no haya pantalla.
  guardarEnHistorial(l.pulso);
  if (!pantallaLista) return;

  oled.clearDisplay();

  // Estado general en grande
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.print(textoNivel(ev.nivel));

  // Acción recomendada
  oled.setTextSize(1);
  oled.setCursor(0, 18);
  oled.print(ev.accion);

  // Pulso actual a la izquierda de la gráfica
  oled.setCursor(0, 40);
  oled.print((int)l.pulso);
  oled.setCursor(0, 50);
  oled.print("bpm");

  dibujarGrafica();
  oled.display();
}

void dibujarGrafica() {
  if (!pantallaLista) return;

  // Limpia solo la zona de la gráfica
  oled.fillRect(GRAFICA_X, GRAFICA_Y, ANCHO_OLED - GRAFICA_X, GRAFICA_ALTO, SSD1306_BLACK);
  if (cantidadGuardada < 2) return;

  // Recorre del valor más viejo al más nuevo y une cada punto con el anterior.
  int inicio = (indiceSiguiente - cantidadGuardada + TAM_HISTORIAL) % TAM_HISTORIAL;
  int yAnterior = valorAY(historialPulso[inicio]);
  for (int i = 1; i < cantidadGuardada; i++) {
    int indice = (inicio + i) % TAM_HISTORIAL;
    int y = valorAY(historialPulso[indice]);
    oled.drawLine(GRAFICA_X + i - 1, yAnterior, GRAFICA_X + i, y, SSD1306_WHITE);
    yAnterior = y;
  }
}
