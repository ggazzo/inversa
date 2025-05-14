#include <Arduino.h>

#ifndef S3_MINI
#define S3_MINI



#define HAS_CUSTOM_SPI_PINS

#define SENSOR_PIN A1
#define HEATER_PIN 4




#define SD_CS_PIN SS
#define SD_MISO MISO
#define SD_MOSI MOSI
#define SD_SCK  SCK


#define TX 21
#define RX 20

#define SDA 35
#define SCL 36





// Sensors | SPI
// { 3 2 } | { 1 0 4 5 }
// Serial    | I2C      | ONEWIRE  | FREE
// { 21 20 } | { 10 8 } | {7}      | 6

#endif