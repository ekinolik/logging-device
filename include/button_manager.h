#pragma once

#include <Arduino.h>

class ButtonManager {
    public:
    ButtonManager(uint8_t pin, bool activeLow, uint32_t debounceMs);

    void begin();
    void update();

    bool isPressed() const;
    bool wasPressedEvent();
    bool wasReleasedEvent();

    private:
    uint8_t m_pin;
    bool m_activeLow;
    uint32_t m_debounceMs;

    bool m_stablePressed = false;
    bool m_lastRawPressed = false;
    uint32_t m_lastRawChangeMs = 0;

    bool m_pressedEvent = false;
    bool m_releasedEvent = false;

    bool readRawPressed() const;
};