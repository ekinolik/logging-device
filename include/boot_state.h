#pragma once

#include <Arduino.h>
#include <esp_sleep.h>

enum class ShutdownReason {
    NONE = 0,
    BUTTON_HOLD = 1,
    OTA = 2,
    LOW_BATTERY = 3,
    SOFTWARE_REQUEST = 4
};

struct BootStateData {
    uint32_t magic;
    uint32_t bootCount;
    ShutdownReason lastShutdownReason;
};

class BootState {
    public:
    static void begin();

    static void setShutdownReason(ShutdownReason reason);
    static ShutdownReason getShutdownReason();

    static uint32_t getBootCount();
    static esp_sleep_wakeup_cause_t getWakeupCause();

    static void printBootSummary(Stream& out);

    private:
    static constexpr uint32_t MAGIC = 0xDEADBEEF;
    static void initializeIfNeeded();

    static const char* shutdownReasonToString(ShutdownReason reason);
    static String wakeupCauseToString(esp_sleep_wakeup_cause_t cause);
};