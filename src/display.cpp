#include "display.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SSD1306 driver implementation
class SSD1306Driver : public DisplayDriver {
private:
    Adafruit_SSD1306 display;

public:
    SSD1306Driver() : display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1) {}

    void init() override {
        if(!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDRESS)) {
            Serial.println(F("SSD1306 allocation failed"));
            return;
        }
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.display();
    }

    void clear() override {
        display.clearDisplay();
    }

    void setContrast(uint8_t contrast) override {
        display.setContrast(contrast);
    }

    void display() override {
        display.display();
    }

    void setTextSize(uint8_t size) override {
        display.setTextSize(size);
    }

    void setTextColor(uint16_t color) override {
        display.setTextColor(color);
    }

    void setCursor(int16_t x, int16_t y) override {
        display.setCursor(x, y);
    }

    void print(const char* text) override {
        display.print(text);
    }

    void println(const char* text) override {
        display.println(text);
    }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        display.drawRect(x, y, w, h, color);
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        display.fillRect(x, y, w, h, color);
    }

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
        display.drawLine(x0, y0, x1, y1, color);
    }

    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override {
        display.drawCircle(x0, y0, r, color);
    }

    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override {
        display.fillCircle(x0, y0, r, color);
    }
};

// Display factory implementation
DisplayDriver* DisplayFactory::createDisplay() {
#ifdef DISPLAY_SSD1306
    return new SSD1306Driver();
#else
    return nullptr;
#endif
} 