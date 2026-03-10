#pragma once
#include <Arduino.h>

// ─── ESP32-C3 Mini Pin Definitions ──────────────────────────
// Pin layout:
//   NTC: A3 (GPIO3)
//   SSR: GPIO2
//   Pump: GPIO6
//   SD:  Custom SPI (CS=5, SCK=1, MISO=0, MOSI=4)
//   I2C: SDA=GPIO8, SCL=GPIO10
//   NeoPixel: GPIO7

// Analog
#define PIN_NTC           3       // A3

// Outputs
#define PIN_HEATER_SSR    2
#define PIN_PUMP_RELAY    6
#define PIN_NEOPIXEL      7

// SD Card (custom SPI pins)
#define PIN_SD_CS         5
#define PIN_SD_SCK        1
#define PIN_SD_MISO       0
#define PIN_SD_MOSI       4
#define HAS_CUSTOM_SPI_PINS 1

// I2C
#define PIN_I2C_SDA       8
#define PIN_I2C_SCL       10

// Serial
#define PIN_SERIAL_TX     21
#define PIN_SERIAL_RX     20
