#include "app_config.h"
#include "button_manager.h"
#include "led_manager.h"
#include "power_manager.h"
#include "boot_state.h"
#include "sd_logger.h"
#include "espnow_receiver.h"

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

EspNowReceiver espNowReceiver;

struct DeviceSequenceState {
    String deviceID;
    uint32_t lastSequence;
    bool used = false;
};

static constexpr size_t MAX_DEVICES = 16;
DeviceSequenceState g_deviceSeq[MAX_DEVICES];

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

    if (!espNowReceiver.begin()) {
        Serial.println("[MAIN] ESPNOW Receiver init failed");
        return;
    }
    Serial.println("[MAIN] ESPNOW Receiver init OK");

    powerManager.setShutdownHandler([]() -> bool {
        logger.beginShutdown();
        logger.update();
        return logger.isIdle();
    });

    // fake startup log
    SDLogger::LogEntry entry;
    entry.deviceId = "logger";
    entry.timestampMs = millis();
    entry.line = "{\"event\":\"boot\",\"boot_count\":" + String(BootState::getBootCount()) + '}';
    logger.appendNow(entry);
}

void loop() {
    espNowReceiver.update();

    EspNowReceiver::ReceivedMessage rx;

    while (espNowReceiver.dequeueMessage(rx)) {
        bool accepted = false;

        if (logger.isAcceptingEntries()) {
            if (isDuplicate(rx.msg.deviceID, rx.msg.sequence)) {
                Serial.printf("[MAIN] Duplicate packet from %s seq=%lu\n",
                    rx.msg.deviceID,
                    (unsigned long)rx.msg.sequence
                );
                accepted = true; // already handled earlier but ACK again
            }
            SDLogger::LogEntry entry;
            entry.deviceId = String(rx.msg.deviceID);
            entry.timestampMs = rx.msg.timestampMs;
            entry.line = String(rx.msg.jsonLine);

            accepted = logger.enqueue(entry);

            if (accepted) {
                markSeen(rx.msg.deviceID, rx.msg.sequence);
            } else {
                Serial.printf("[MAIN] Logger queue full, rejecting packet %s seq=%lu\n",
                    rx.msg.deviceID,
                    (unsigned long)rx.msg.sequence
                );
            }
        } else {
            Serial.println("[MAIN] Not accepting entries, rejecting packet");
        }

        if (!espNowReceiver.sendAck(rx.mac, rx.msg.deviceID, rx.msg.sequence, accepted)) {
            Serial.println("[MAIN] Failed to send ACK");
        }

        if (!accepted) {
            Serial.println("[MAIN] Logger rejected packet; NACK sent");
        }
    }

    logger.update();

    button.update();
    powerManager.update();
    led.update();

    if (!powerManager.isAcceptingRequests()) {
        logger.beginShutdown();
    }

    delay(1);

    /*
    logger.update();

    // temporary for phase 2 test generator
    static uint32_t lastWriteMs = 0;
    static uint32_t counter = 0;
    const uint32_t now = millis();

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
        Serial.println("[MAIN] Failed to enqueue test log 1");
    }

    if (!logger.enqueue(entry)) {
        Serial.println("[MAIN] Failed to enqueue test log 2");
    }

    if (!logger.enqueue(entry)) {
        Serial.println("[MAIN] Failed to enqueue test log 3");
    }

    if (!logger.enqueue(entry)) {
        Serial.println("[MAIN] Failed to enqueue test log 4");
    }

    if (!logger.enqueue(entry)) {
        Serial.println("[MAIN] Failed to enqueue test log 5");
    }
    */
}

bool isDuplicate(const char* deviceID, uint32_t sequence) {
    for (size_t i = 0; i < MAX_DEVICES; i++) {
        if (!g_deviceSeq[i].used) continue;
        if (g_deviceSeq[i].deviceID == deviceID) {
            return sequence <= g_deviceSeq[i].lastSequence;
        }
    }

    return false;
}

void markSeen(const char* deviceID, uint32_t sequence) {
    for (size_t i = 0; i < MAX_DEVICES; i++) {
        if (g_deviceSeq[i].used && g_deviceSeq[i].deviceID == deviceID) {
            g_deviceSeq[i].lastSequence = sequence;

            return;
        }
    }

    for (size_t i = 0; i < MAX_DEVICES; i++) {
        if (!g_deviceSeq[i].used) {
            g_deviceSeq[i].used = true;
            g_deviceSeq[i].deviceID = deviceID;
            g_deviceSeq[i].lastSequence = sequence;

            return;
        }
    }
}