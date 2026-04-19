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

    static constexpr bool LOGGING_REQUIRED = false;

    // SD Card
    static constexpr gpio_num_t SD_CS_PIN = GPIO_NUM_20;
    static constexpr gpio_num_t SD_MOSI_PIN = GPIO_NUM_19;
    static constexpr gpio_num_t SD_SCK_PIN = GPIO_NUM_18;
    static constexpr gpio_num_t SD_MISO_PIN = GPIO_NUM_15;

    static constexpr uint32_t SD_SPI_FREQUENCY = 8000000;
    static constexpr size_t SD_MAX_QUEUE_SIZE = 32;

    // ESPNOW
    static constexpr uint8_t ESPNOW_CHANNEL = 1;
    static constexpr size_t ESPNOW_RX_QUEUE_SIZE = 8;
    static constexpr bool ESPNOW_REQUIRE_KNOWN_PEERS = false; // start open

    static constexpr uint32_t ESPNOW_ACK_TIMEOUT_MS = 400;
    static constexpr uint8_t ESPNOW_MAX_RETRIES = 5;

}