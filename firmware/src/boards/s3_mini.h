#pragma once
#include <Arduino.h>

// ─── ESP32-S3 Mini Pin Definitions ──────────────────────────
// Pin layout:
//   NTC: A1 (GPIO1)
//   SSR: GPIO4
//   Pump: GPIO16
//   SD:  Default SPI (SS, SCK, MISO, MOSI)
//   I2C: SDA=GPIO35, SCL=GPIO36
//   NeoPixel: GPIO47

// Analog
#define PIN_NTC           1       // A1

// Outputs
#define PIN_HEATER_SSR    4
#define PIN_PUMP_RELAY    16
#define PIN_NEOPIXEL      47

// Zero-cross detector input (only used when HEATER_DRIVER == HEATER_DRIVER_BURST_FIRE).
// Placeholder GPIO: revise once the opto ZC circuit is wired on the production board.
#define PIN_HEATER_ZC     17

// SD Card (default SPI)
#define PIN_SD_CS         SS
#define PIN_SD_SCK        SCK
#define PIN_SD_MISO       MISO
#define PIN_SD_MOSI       MOSI
#define HAS_CUSTOM_SPI_PINS 0

// I2C
#define PIN_I2C_SDA       35
#define PIN_I2C_SCL       36

// Serial
#define PIN_SERIAL_TX     21
#define PIN_SERIAL_RX     20
