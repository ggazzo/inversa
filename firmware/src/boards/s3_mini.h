#pragma once
#include <Arduino.h>

// ─── ESP32-S3 Mini Pin Definitions ──────────────────────────
// Pin layout:
//   NTC: A1 (GPIO2)
//   SSR: GPIO4
//   Pump: GPIO16
//   SD:  Default SPI (SS, SCK, MISO, MOSI)
//   I2C: SDA=GPIO35, SCL=GPIO36
//   NeoPixel: GPIO47

// Analog
// `A1` is the silkscreen label on the Lolin S3 Mini; per the
// arduino-esp32 lolin_s3_mini variant that maps to GPIO 2 (ADC1_CH1),
// not GPIO 1. Older firmware hardcoded `1`, which read a floating
// neighbouring pin and tripped the watchdog with SENSOR_FAULT in ~10 s.
#define PIN_NTC           2       // A1 → GPIO2 (ADC1_CH1)

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
