// display.h — SSD1306 OLED on the astronaut sensor.
#pragma once

// ===== Display hardware =====
// The OLED must be powered from 3V3, not 5V: the ESP32's I2C lines are 3.3 V,
// and many SSD1306 modules pass VCC straight to the I2C pull-ups.
constexpr int OLED_WIDTH = 128;               // Display width (pixels)
constexpr int OLED_HEIGHT = 64;               // Display height (pixels)
constexpr unsigned char OLED_ADDRESS = 0x3C;  // Usual I2C address; some modules use 0x3D
constexpr int OLED_SDA_PIN = 21;              // I2C data (GPIO)
constexpr int OLED_SCL_PIN = 22;              // I2C clock (GPIO)

// Starts I2C and the SSD1306 and clears the screen. Tries OLED_ADDRESS first,
// then 0x3D. Returns false (and prints why) if the display does not respond.
bool initDisplay();

// Draws "Hello" (size 2) and "world, Pulsar" (size 1).
void showHello();
