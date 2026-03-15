#include "app_config.h"
#include "button_manager.h"
#include "led_manager.h"
#include "power_manager.h"
#include "boot_state.h"
#include "sd_logger.h"

#include <Arduino.h>
#include <SPI.h>

ButtonManager button(
    static_cast<uint8_t>(AppConfig::BUTTON_PIN),
    AppConfig::BUTTON_ACTIVE_LOW,
    AppConfig::BUTTON_DEBOUNCE_MS
);

LedManager led(static_cast<uint8_t>(AppConfig::LED_PIN));

PowerManager powerManager(
    button,
    led,
    AppConfig::SHUTDOWN_HOLD_MS
);

SPIClass spi = SPIClass(FSPI);
SDLogger logger(spi);

void setup() {
    Serial.begin(115200);
    delay(2000);

    BootState::begin();
    BootState::printBootSummary(Serial);

    //gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);

    button.begin();
    led.begin();
    //led.setMode(LedMode::SOLID_ON);
    powerManager.begin();

    Serial.println("Boot Complete");
    Serial.println("Starting Setup...");

    if (!logger.begin()) {
        Serial.println("[MAIN] SD Logger init failed");
        return;
    }

    Serial.println("[MAIN] SD Logger init OK");

    powerManager.setShutdownHandler([]() -> bool {
        logger.beginShutdown();
        logger.update();
        return logger.isIdle();
    });

    // fake startup log
    SDLogger::LogEntry entry;
    entry.deviceId = "logger";
    entry.timestampMs = millis();
    entry.line = "{\"event\":\"boot\",\"boot_count\":" + BootState::getBootCount() + '}';
    logger.appendNow(entry);
}

void loop() {
    delay(1);

    button.update();
    powerManager.update();
    led.update();

    if (!powerManager.isAcceptingRequests()) {
        logger.beginShutdown();
    }

    logger.update();

    // temporary for phase 2 test generator
    static uint32_t lastWriteMs = 0;
    static uint32_t counter = 0;
    static uint32_t now = millis();

    if (!powerManager.isAcceptingRequests() || !logger.isAcceptingEntries() || now - lastWriteMs < 5000) {
        return;
    }

    lastWriteMs = now;

    SDLogger::LogEntry entry;
    entry.deviceId = "test_sensor_1";
    entry.timestampMs = now;
    entry.line = 
        "{\"ts_ms\":" + String(now) + 
        ",\"type\":\"test\"" + 
        ",\"counter\":" + String(counter++) + '}';

    if (!logger.enqueue(entry)) {
        Serial.println("[MAIN] Failed to enqueue test log");
    }
}