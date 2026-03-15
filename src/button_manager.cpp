#include "button_manager.h"

ButtonManager::ButtonManager(uint8_t pin, bool activeLow, uint32_t debounceMs) :
    m_pin(pin), m_activeLow(activeLow), m_debounceMs(debounceMs) {}

void ButtonManager::begin() {
    if (m_activeLow) {
        Serial.println("[BUTTON] Configuring with pull-up");
        pinMode(m_pin, INPUT_PULLUP);
    } else {
        Serial.println("[BUTTON] Configuring with pull-down");
        pinMode(m_pin, INPUT_PULLDOWN);
    }

    m_lastRawPressed = readRawPressed();
    m_stablePressed = m_lastRawPressed;
    m_lastRawChangeMs = millis();
}

void ButtonManager::update() {
    const uint32_t now = millis();
    const bool rawPressed = readRawPressed();

    if (rawPressed != m_lastRawPressed) {
        m_lastRawPressed = rawPressed;
        m_lastRawChangeMs = now;
    }

    if ((now - m_lastRawChangeMs) >= m_debounceMs && rawPressed != m_stablePressed) {
        m_stablePressed = rawPressed;

        if (m_stablePressed) {
            m_pressedEvent = true;
        } else {
            m_releasedEvent = true;
        }
    }
}

bool ButtonManager::isPressed() const {
    return m_stablePressed;
}

bool ButtonManager::wasPressedEvent() {
    const bool result = m_pressedEvent;
    m_pressedEvent = false;
    return result;
}

bool ButtonManager::wasReleasedEvent() {
    const bool result = m_releasedEvent;
    m_releasedEvent = false;
    return result;
}

bool ButtonManager::readRawPressed() const {
    const int value = digitalRead(m_pin);

    // For active-low buttons, LOW means pressed. For active-high buttons, HIGH means pressed.
    if (m_activeLow) {
        return value == LOW;
    }

    return value == HIGH;
}