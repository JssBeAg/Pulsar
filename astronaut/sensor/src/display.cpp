// display.cpp — OLED display implementation.
//
// Screen layout (128 x 64 pixels):
//   y 0–15  : overall level in large text ("GREEN", "YELLOW", "RED")
//   y 18–25 : recommended action (one line)
//   y 28–63 : current pulse on the left, chart on the right
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display.h"
#include "config.h"

// ===== Hardware =====
static const int OLED_WIDTH = 128;
static const int OLED_HEIGHT = 64;
static const uint8_t OLED_ADDRESS = 0x3C;
static const int8_t NO_RESET_PIN = -1;  // The module shares the ESP32 reset

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, NO_RESET_PIN);
static bool displayReady = false;

// ===== Ring buffer with the latest pulse values =====
static const int HISTORY_SIZE = 100;
static float pulseHistory[HISTORY_SIZE];
static int nextIndex = 0;    // Position where the next value is written
static int storedCount = 0;  // How many valid values there are (at most HISTORY_SIZE)

// ===== Chart area =====
static const int CHART_X = 28;       // 128 - 28 = 100 pixels: one per value
static const int CHART_Y = 28;
static const int CHART_HEIGHT = 36;  // 28 + 36 = 64
static const float CHART_MIN = 40.0f;   // Same range the potentiometer simulates
static const float CHART_MAX = 180.0f;

// Level text shown in large letters.
static const char* levelText(Level level) {
  switch (level) {
    case GREEN:  return "GREEN";
    case YELLOW: return "YELLOW";
    case RED:    return "RED";
  }
  return "?";
}

// Stores a value in the ring buffer (the oldest is overwritten).
static void addToHistory(float pulse) {
  pulseHistory[nextIndex] = pulse;
  nextIndex = (nextIndex + 1) % HISTORY_SIZE;
  if (storedCount < HISTORY_SIZE) storedCount++;
}

// Converts a pulse value to its y coordinate inside the chart.
static int valueToY(float value) {
  float ratio = (value - CHART_MIN) / (CHART_MAX - CHART_MIN);
  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  // On the display y grows downward, so it is inverted.
  return CHART_Y + (CHART_HEIGHT - 1) - (int)(ratio * (CHART_HEIGHT - 1));
}

void initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  displayReady = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (!displayReady) {
    Serial.println("[display] OLED not found. Continuing without a display.");
    return;
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);  // The action uses a single line; anything extra is cut off
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.print("PULSAR");
  oled.display();
}

void showStatus(const Evaluation& ev, const Readings& r) {
  // The history is stored even without a display.
  addToHistory(r.pulse);
  if (!displayReady) return;

  oled.clearDisplay();

  // Overall status in large text
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.print(levelText(ev.level));

  // Recommended action
  oled.setTextSize(1);
  oled.setCursor(0, 18);
  oled.print(ev.action);

  // Current pulse to the left of the chart
  oled.setCursor(0, 40);
  oled.print((int)r.pulse);
  oled.setCursor(0, 50);
  oled.print("bpm");

  drawChart();
  oled.display();
}

void drawChart() {
  if (!displayReady) return;

  // Clear only the chart area
  oled.fillRect(CHART_X, CHART_Y, OLED_WIDTH - CHART_X, CHART_HEIGHT, SSD1306_BLACK);
  if (storedCount < 2) return;

  // Walk from the oldest value to the newest and join each point to the previous one.
  int start = (nextIndex - storedCount + HISTORY_SIZE) % HISTORY_SIZE;
  int previousY = valueToY(pulseHistory[start]);
  for (int i = 1; i < storedCount; i++) {
    int index = (start + i) % HISTORY_SIZE;
    int y = valueToY(pulseHistory[index]);
    oled.drawLine(CHART_X + i - 1, previousY, CHART_X + i, y, SSD1306_WHITE);
    previousY = y;
  }
}
