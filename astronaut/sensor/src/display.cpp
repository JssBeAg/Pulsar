// display.cpp — SSD1306 OLED driver for the astronaut sensor.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display.h"

// Some SSD1306 boards are strapped to 0x3D instead of 0x3C.
static const unsigned char OLED_FALLBACK_ADDRESS = 0x3D;
static const int8_t NO_RESET_PIN = -1;  // The 4-pin module has no reset line

static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, NO_RESET_PIN);
static bool displayReady = false;
static bool linkDotOn = false;  // Last state passed to drawLinkIndicator; showScreen redraws it

// Draws the dot into the buffer only; the caller sends it with display().
static void drawLinkDot() {
  // Clear the corner first, so going from filled to outlined erases the inside.
  display.fillRect(LINK_DOT_X, LINK_DOT_Y, LINK_DOT_SIZE, LINK_DOT_SIZE, SSD1306_BLACK);
  if (linkDotOn) {
    display.fillRect(LINK_DOT_X, LINK_DOT_Y, LINK_DOT_SIZE, LINK_DOT_SIZE, SSD1306_WHITE);
  } else {
    display.drawRect(LINK_DOT_X, LINK_DOT_Y, LINK_DOT_SIZE, LINK_DOT_SIZE, SSD1306_WHITE);
  }
}

void drawLinkIndicator(bool on) {
  linkDotOn = on;
  if (!displayReady) return;
  drawLinkDot();
  display.display();
}

bool initDisplay() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  // The I2C address depends on how the module is built (a solder jumper on
  // the back), so try the usual one first and fall back to the other.
  unsigned char address = OLED_ADDRESS;
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, address);
  if (!displayReady) {
    address = OLED_FALLBACK_ADDRESS;
    displayReady = display.begin(SSD1306_SWITCHCAPVCC, address);
  }

  if (!displayReady) {
    Serial.printf("[display] ERROR: no SSD1306 at 0x%02X or 0x%02X. Check 3V3, GND, SDA=GPIO %d, SCL=GPIO %d.\n",
                  OLED_ADDRESS, OLED_FALLBACK_ADDRESS, OLED_SDA_PIN, OLED_SCL_PIN);
    return false;
  }

  Serial.printf("[display] SSD1306 found at 0x%02X\n", address);
  display.clearDisplay();
  display.display();
  return true;
}

void showScreen(Screen screen) {
  if (!displayReady) return;

  // Drawing only changes the library's buffer in RAM. Nothing reaches the
  // screen until display() sends that buffer over I2C, so it is called once,
  // after everything is drawn.
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(screen == SCREEN_HELLO ? "Hello" : "Bye");

  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("world, Pulsar");

  // clearDisplay() above erased the dot; put it back in its current state.
  drawLinkDot();
  display.display();
}
