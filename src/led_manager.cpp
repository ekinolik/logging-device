#include "led_manager.h"
#include "app_config.h"

LedManager::LedManager(uint8_t pin) : m_pin(pin) {}

void LedManager::begin() {
    pinMode(m_pin, OUTPUT);
    setMode(LedMode::OFF);
}

void LedManager::setMode(LedMode mode) {
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    m_lastToggleMs = millis();

    switch(m_mode) {
        case LedMode::OFF:
            writeOutput(false);
            break;

        case LedMode::SOLID_ON:
            writeOutput(true);
            break;

        case LedMode::FAST_BLINK:
        case LedMode::SLOW_BLINK:
            writeOutput(true);
            break;
    }
}

LedMode LedManager::getMode()  const {
    return m_mode; 
}

void LedManager::update() {
    const uint32_t now = millis();

    switch(m_mode) {
        case LedMode::OFF:
        case LedMode::SOLID_ON:
            return;

        case LedMode::FAST_BLINK:
            if (now - m_lastToggleMs >= AppConfig::LED_FAST_BLINK_MS) {
                m_lastToggleMs = now;
                writeOutput(!m_outputState);
            }
            break;

        case LedMode::SLOW_BLINK:
            if (now - m_lastToggleMs >= AppConfig::LED_SLOW_BLINK_MS) {
                m_lastToggleMs = now;
                writeOutput(!m_outputState);
            }
            break;
    }

}

void LedManager::writeOutput(bool on) {
    m_outputState = on;
    digitalWrite(m_pin, on ? HIGH : LOW);
}