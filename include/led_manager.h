#pragma once

#include <Arduino.h>

enum class LedMode {
    OFF,
    SOLID_ON,
    FAST_BLINK,
    SLOW_BLINK
};

class LedManager {
public:
    explicit LedManager(uint8_t pin);

    void begin();
    void setMode(LedMode mode);
    LedMode getMode() const;

    void update();

private:
    uint8_t m_pin;
    LedMode m_mode = LedMode::OFF;
    bool m_outputState = false;
    uint32_t m_lastToggleMs = 0;

    void writeOutput(bool on);
};
