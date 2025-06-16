#include <Arduino.h>

#ifndef C3_MINI
#define C3_MINI



#define PIN_NEOPIXEL 7

#define HAS_CUSTOM_SPI_PINS

#define SENSOR_PIN A3
#define HEATER_PIN 2

#define SD_CS_PIN SS
#define SD_MISO MISO
#define SD_MOSI MOSI
#define SD_SCK  SCK

#define TX 21
#define RX 20

#define SDA 8
#define SCL 10


#define PUMP_PIN 6



// Sensors | SPI
// { 3 2 } | { 1 0 4 5 }
// Serial    | I2C      | ONEWIRE  | FREE
// { 21 20 } | { 10 8 } | {7}      | 6

#endif