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

// The screens the sensor can show.
enum Screen { SCREEN_HELLO, SCREEN_BYE };

// Starts I2C and the SSD1306 and clears the screen. Tries OLED_ADDRESS first,
// then 0x3D. Returns false (and prints why) if the display does not respond.
bool initDisplay();

// ===== Link dot =====
// A small square in the top-right corner: filled when the station is
// acknowledging our data, outlined when it is not.
constexpr int LINK_DOT_SIZE = 5;                          // Side of the dot (pixels)
constexpr int LINK_DOT_X = OLED_WIDTH - LINK_DOT_SIZE;    // Left edge of the dot (pixels)
constexpr int LINK_DOT_Y = 0;                             // Top edge of the dot (pixels)

// Draws the chosen screen:
//   SCREEN_HELLO → "Hello" (size 2) + "world, Pulsar" (size 1)
//   SCREEN_BYE   → "Bye"   (size 2) + "world, Pulsar" (size 1)
// The link dot is redrawn on top in its last state.
void showScreen(Screen screen);

// Draws the link dot (filled = on, outlined = off) and sends it to the
// screen. The caller decides when: only on a state change.
void drawLinkIndicator(bool on);
