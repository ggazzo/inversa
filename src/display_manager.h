#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "display.h"
#include "state.h"

class DisplayManager {
private:
    DisplayDriver* display;
    bool enabled;
    uint8_t contrast;
    uint8_t textSize;
    uint16_t textColor;

public:
    DisplayManager();
    ~DisplayManager();

    void init();
    void update(const MachineState& state);
    void setEnabled(bool enabled);
    void setContrast(uint8_t contrast);
    void setTextSize(uint8_t size);
    void setTextColor(uint16_t color);
    void clear();
    void display();

    // UI update methods
    void updateTemperature(const MachineState& state);
    void updateTargetTemperature(const MachineState& state);
    void updateOutputLevel(const MachineState& state);
    void updateStatus(const MachineState& state);
    void updateMenu(const MachineState& state);
    void updateError(const MachineState& state);
};

#endif // DISPLAY_MANAGER_H 