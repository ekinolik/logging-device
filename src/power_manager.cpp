#include "power_manager.h"
#include "app_config.h"
#include "boot_state.h"
#include <esp_sleep.h>

PowerManager::PowerManager(ButtonManager& button, LedManager& led, uint32_t shutdownHoldMs) :
    m_button(button),
    m_led(led),
    m_shutdownHoldMs(shutdownHoldMs),
    m_state(PowerState::ON),
    m_acceptingRequests(true),
    m_pressStartMs(0) {}

void PowerManager::begin() {
    setState(PowerState::ON);
}

void PowerManager::update() {
    const uint32_t now = millis();

    switch(m_state) {
        case PowerState::ON:
            handleOnState(now);
            break;

        case PowerState::PREPARING_SHUTDOWN:
            handlePreparingShutdownState(now);
            break;

        case PowerState::READY_FOR_SHUTDOWN:
            handleReadyForShutdownState(now);
            break;

        case PowerState::SHUTTING_DOWN:
            performShutdown();
            break;

        case PowerState::OFF:
            break;
    }
}

PowerState PowerManager::getState() const {
    return m_state;
}

bool PowerManager::isAcceptingRequests() const {
    return m_acceptingRequests;
}

void PowerManager::setState(PowerState newState) {
    m_state = newState;

    Serial.print("[POWER] ");
    Serial.print((int)m_state);
    Serial.print(" -> ");
    Serial.println((int)newState);

    switch(m_state) {
        case PowerState::ON:
            m_acceptingRequests = true;
            m_led.setMode(LedMode::SOLID_ON);
            break;

        case PowerState::PREPARING_SHUTDOWN:
            m_acceptingRequests = false;
            m_led.setMode(LedMode::FAST_BLINK);
            break;

        case PowerState::READY_FOR_SHUTDOWN:
            m_acceptingRequests = false;
            m_led.setMode(LedMode::SLOW_BLINK);
            break;

        case PowerState::SHUTTING_DOWN:
            m_acceptingRequests = false;
            break;

        case PowerState::OFF:
            m_acceptingRequests = false;
            m_led.setMode(LedMode::OFF);
            break;
    }
}

void PowerManager::handleOnState(uint32_t now) {
    (void)now;

    if (m_button.wasPressedEvent()) {
        m_pressStartMs = millis();
        setState(PowerState::PREPARING_SHUTDOWN);
    }
}

void PowerManager::handlePreparingShutdownState(uint32_t now) {
    if (m_button.wasReleasedEvent()) {
        setState(PowerState::ON);
        return;
    }

    if (m_button.isPressed() && (now - m_pressStartMs >= m_shutdownHoldMs)) {
        setState(PowerState::READY_FOR_SHUTDOWN);
    }
}

void PowerManager::handleReadyForShutdownState(uint32_t now) {
    (void)now;

    if (m_button.wasReleasedEvent()) {
        setState(PowerState::SHUTTING_DOWN);
    }
}

void PowerManager::performShutdown() {
    m_led.setMode(LedMode::OFF);
    m_led.update();

    Serial.println("[POWER] Preparing for deep sleep");

    // Wait for button to be released and stable before sleeping, to avoid immediately waking up again
    uint32_t stableShutdownStart = millis();
    while (millis() - stableShutdownStart < AppConfig::STABLE_SHUTDOWN_RELEASE_MS) {
        if (digitalRead(AppConfig::BUTTON_PIN) == (AppConfig::BUTTON_ACTIVE_LOW ? LOW : HIGH)) {
            stableShutdownStart = millis();
        }
        delay(10);
    }

    BootState::setShutdownReason(ShutdownReason::BUTTON_HOLD);

    // Wake again when button is pressed (LOW for active-low button)
    esp_deep_sleep_enable_gpio_wakeup(
        1ULL << (uint64_t)AppConfig::BUTTON_PIN,
        ESP_GPIO_WAKEUP_GPIO_LOW
    );

    Serial.flush();
    m_state = PowerState::OFF;
    esp_deep_sleep_start();
}