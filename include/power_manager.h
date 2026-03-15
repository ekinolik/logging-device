#pragma once

#include <Arduino.h>
#include "button_manager.h"
#include "led_manager.h"

enum class PowerState {
    ON,
    PREPARING_SHUTDOWN,
    READY_FOR_SHUTDOWN,
    SHUTTING_DOWN,
    OFF
};

class PowerManager {
    public:
    PowerManager(ButtonManager& button, LedManager& led, uint32_t shutdownHoldMs);

    void begin();
    void update();

    PowerState getState() const;
    bool isAcceptingRequests() const;

    private:
    ButtonManager& m_button;
    LedManager& m_led;
    uint32_t m_shutdownHoldMs;

    PowerState m_state;
    bool m_acceptingRequests;
    uint32_t m_pressStartMs;

    void setState(PowerState newState);
    void handleOnState(uint32_t now);
    void handlePreparingShutdownState(uint32_t now);
    void handleReadyForShutdownState(uint32_t now);
    void performShutdown();
};