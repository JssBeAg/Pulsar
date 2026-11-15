// display.h — SSD1306 OLED display (128x64, I2C, address 0x3C).
#pragma once
#include "readings.h"
#include "rules.h"

// Starts I2C and the display. If the display does not respond, the rest of
// the system keeps working (it only reports it over serial).
void initDisplay();

// Shows the overall status (GREEN/YELLOW/RED in large text), the action
// on one line, and the pulse chart. Also stores the pulse in the history.
// `ev` must be the evaluation with the worst level (the one in charge).
void showStatus(const Evaluation& ev, const Readings& r);

// Draws the line chart of the last 100 pulse values at the bottom of the
// display. It only draws in memory: showStatus() calls it and then sends
// everything to the display.
void drawChart();
