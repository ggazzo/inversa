#include <Arduino.h>

#ifndef S3_MINI
#define S3_MINI



#define HAS_CUSTOM_SPI_PINS

#define SENSOR_PIN A3
#define HEATER_PIN 2

#define SD_CS_PIN 10
#define SD_MISO 13
#define SD_MOSI 12
#define SD_SCK  11


#define TX 21
#define RX 20

#define SDA 35
#define SCL 36





// Sensors | SPI
// { 3 2 } | { 1 0 4 5 }
// Serial    | I2C      | ONEWIRE  | FREE
// { 21 20 } | { 10 8 } | {7}      | 6

#endif