#pragma once

#include <Arduino.h>

namespace AppConfig {
    static constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_6;
    static constexpr gpio_num_t LED_PIN    = GPIO_NUM_16;

    static constexpr bool BUTTON_ACTIVE_LOW = true;

    static constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
    static constexpr uint32_t SHUTDOWN_HOLD_MS = 3000;

    static constexpr uint32_t LED_FAST_BLINK_MS = 120;
    static constexpr uint32_t LED_SLOW_BLINK_MS = 500;

    static constexpr uint32_t STABLE_SHUTDOWN_RELEASE_MS = 150;
}