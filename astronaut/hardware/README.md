# Astronaut device hardware

Wiring and parts list for the ESP32 sensor (`astronaut/sensor/`). Text only for now; a diagram may come later.

## Parts

- 1 × **ESP32** DevKit (classic `esp32dev`)
- 5 × **B10K linear potentiometers** (they simulate the sensors)
- 1 × **SSD1306 OLED**, 128×64, I2C (address `0x3C`)
- Breadboard, jumper wires and a USB **data** cable

## Power rails

- Connect the ESP32 **3.3V** pin to the breadboard's **+** rail.
- Connect the ESP32 **GND** pin to the breadboard's **−** rail.
- Everything below takes power from these two rails. Do not use 5V: the ESP32 ADC reads 0–3.3 V only.

## Wiring

| Component | Pin | ESP32 |
|---|---|---|
| Pulse potentiometer | middle leg | GPIO 34 |
| SpO₂ potentiometer | middle leg | GPIO 35 |
| Temperature potentiometer | middle leg | GPIO 32 |
| CO₂ potentiometer | middle leg | GPIO 33 |
| Radiation potentiometer | middle leg | GPIO 36 |
| Every potentiometer | outer legs | 3.3V rail and GND rail |
| OLED | SDA | GPIO 21 |
| OLED | SCL | GPIO 22 |
| OLED | VCC / GND | 3.3V rail / GND rail |

> Only **ADC1** pins (GPIO 32–39) are used because **ADC2 does not work while WiFi is on**. GPIO 39 is free for another sensor.

The pins are defined in `astronaut/sensor/src/config.h`. If you rewire, change them there.
