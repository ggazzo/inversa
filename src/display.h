#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Wire.h>

// Display type definitions
#define DISPLAY_SSD1315
// #define DISPLAY_SSD1306
// #define DISPLAY_SH1106

// Display configuration
#ifdef DISPLAY_SSD1315
    #define DISPLAY_WIDTH 128
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_I2C_ADDRESS 0x3C
#elif defined(DISPLAY_SSD1306)
    #define DISPLAY_WIDTH 128
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_I2C_ADDRESS 0x3C
#elif defined(DISPLAY_SH1106)
    #define DISPLAY_WIDTH 128
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_I2C_ADDRESS 0x3C
#else
    #error "No display type defined. Please define DISPLAY_SSD1315, DISPLAY_SSD1306, or DISPLAY_SH1106"
#endif

// Base display driver class
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;
    virtual void init() = 0;
    virtual void clear() = 0;
    virtual void setContrast(uint8_t contrast) = 0;
    virtual void display() = 0;
    virtual void setTextSize(uint8_t size) = 0;
    virtual void setTextColor(uint16_t color) = 0;
    virtual void setCursor(int16_t x, int16_t y) = 0;
    virtual void print(const char* text) = 0;
    virtual void println(const char* text) = 0;
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) = 0;
    virtual void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) = 0;
    virtual void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) = 0;
};

// Display factory class
class DisplayFactory {
public:
    static DisplayDriver* createDisplay();
};

#endif // DISPLAY_H 