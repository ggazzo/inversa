#include "display_manager.h"

DisplayManager::DisplayManager() : display(nullptr), enabled(false), contrast(255), textSize(1), textColor(SSD1306_WHITE) {
    display = DisplayFactory::createDisplay();
}

DisplayManager::~DisplayManager() {
    if (display) {
        delete display;
    }
}

void DisplayManager::init() {
    if (display) {
        display->init();
        display->setContrast(contrast);
        display->setTextSize(textSize);
        display->setTextColor(textColor);
        enabled = true;
    }
}

void DisplayManager::update(const MachineState& state) {
    if (!enabled || !display) return;

    display->clear();
    
    // Update all UI elements
    updateTemperature(state);
    updateTargetTemperature(state);
    updateOutputLevel(state);
    updateStatus(state);
    updateMenu(state);
    updateError(state);
    
    display->display();
}

void DisplayManager::setEnabled(bool enabled) {
    this->enabled = enabled;
}

void DisplayManager::setContrast(uint8_t contrast) {
    this->contrast = contrast;
    if (display) {
        display->setContrast(contrast);
    }
}

void DisplayManager::setTextSize(uint8_t size) {
    this->textSize = size;
    if (display) {
        display->setTextSize(size);
    }
}

void DisplayManager::setTextColor(uint16_t color) {
    this->textColor = color;
    if (display) {
        display->setTextColor(color);
    }
}

void DisplayManager::clear() {
    if (display) {
        display->clear();
    }
}

void DisplayManager::display() {
    if (display) {
        display->display();
    }
}

void DisplayManager::updateTemperature(const MachineState& state) {
    if (!display) return;
    
    display->setTextSize(2);
    display->setCursor(0, 0);
    display->print("Temp: ");
    display->print(state.current_temperature_c, 1);
    display->print("C");
}

void DisplayManager::updateTargetTemperature(const MachineState& state) {
    if (!display) return;
    
    display->setTextSize(2);
    display->setCursor(0, 20);
    display->print("Set: ");
    display->print(state.target_temperature_c, 1);
    display->print("C");
}

void DisplayManager::updateOutputLevel(const MachineState& state) {
    if (!display) return;
    
    // Draw a progress bar for output level
    int barWidth = 100;
    int barHeight = 8;
    int x = 14;
    int y = 40;
    
    // Draw background
    display->drawRect(x, y, barWidth, barHeight, SSD1306_WHITE);
    
    // Draw filled portion
    int fillWidth = (state.output_val * barWidth) / OUTPUT_MAX;
    display->fillRect(x, y, fillWidth, barHeight, SSD1306_WHITE);
    
    // Draw percentage
    display->setTextSize(1);
    display->setCursor(x + barWidth + 5, y);
    display->print((state.output_val * 100) / OUTPUT_MAX);
    display->print("%");
}

void DisplayManager::updateStatus(const MachineState& state) {
    if (!display) return;
    
    display->setTextSize(1);
    display->setCursor(0, 50);
    
    switch (state.current) {
        case StateType::WAIT_TEMPERATURE:
            display->print("Waiting for temp");
            break;
        case StateType::WAIT_TIMER:
            display->print("Timer running");
            break;
        case StateType::WAIT_CONFIRM:
            display->print("Confirm: ");
            display->print(state.confirm_message);
            break;
        case StateType::PREPARING:
            display->print("Preparing");
            break;
        case StateType::IDLE:
            display->print("Idle");
            break;
    }
}

void DisplayManager::updateMenu(const MachineState& state) {
    if (!display) return;
    
    // Menu implementation will be added later
}

void DisplayManager::updateError(const MachineState& state) {
    if (!display) return;
    
    if (state.message[0] != '\0') {
        display->setTextSize(1);
        display->setCursor(0, 56);
        display->print("Error: ");
        display->print(state.message);
    }
} 