#include "boot_state.h"

RTC_DATA_ATTR BootStateData g_bootState = {
    0,
    0,
    ShutdownReason::NONE
};

const char* BootState::shutdownReasonToString(ShutdownReason reason) {
    switch (reason) {
        case ShutdownReason::NONE: return "NONE";
        case ShutdownReason::BUTTON_HOLD: return "BUTTON_HOLD";
        case ShutdownReason::OTA: return "OTA";
        case ShutdownReason::LOW_BATTERY: return "LOW_BATTERY";
        case ShutdownReason::SOFTWARE_REQUEST: return "SOFTWARE_REQUEST";
        default: return "UNKNOWN";
    }
}

String BootState::wakeupCauseToString(esp_sleep_wakeup_cause_t cause) {
    switch (cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED: return "UNDEFINED";
        case ESP_SLEEP_WAKEUP_ALL: return "ALL";
        case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
        case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        default:
            return "UNKNOWN" + String((int)cause) + ")";
    }
}

void BootState::initializeIfNeeded() {
    if (g_bootState.magic != MAGIC) {
        g_bootState.magic = MAGIC;
        g_bootState.bootCount = 0;
        g_bootState.lastShutdownReason = ShutdownReason::NONE;
    }
}

void BootState::begin() {
    initializeIfNeeded();
    g_bootState.bootCount++;
}

void BootState::setShutdownReason(ShutdownReason reason) {
    initializeIfNeeded();
    g_bootState.lastShutdownReason = reason;
}

ShutdownReason BootState::getShutdownReason() {
    initializeIfNeeded();
    return g_bootState.lastShutdownReason;
}

uint32_t BootState::getBootCount() {
    initializeIfNeeded();
    return g_bootState.bootCount;
}

esp_sleep_wakeup_cause_t BootState::getWakeupCause() {
    return esp_sleep_get_wakeup_cause();
}

void BootState::printBootSummary(Stream& out) {
    initializeIfNeeded();

    out.print("[BOOT] Boot count: ");
    out.println(g_bootState.bootCount);

    out.print("[BOOT] Wake cause: ");
    out.println(wakeupCauseToString(esp_sleep_get_wakeup_cause()));

    out.print("[BOOT] Last shutdown reason: ");
    out.println(shutdownReasonToString(g_bootState.lastShutdownReason));

    out.print("[BOOT] Reset reason ");
    out.println((int)esp_reset_reason());
}