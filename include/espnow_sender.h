#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "log_protocol.h"
#include "app_config.h"

class EspNowSender {
public:
    struct PendingLog {
        LogMessageV1 msg;
    };

    EspNowSender();

    bool begin(const uint8_t* loggerMac[6]);
    void update();

    bool enqueueLog(const char* deviceID, uint32_t timestampMs, const char* jsonLine);

    uint32_t sentCount() { return m_sentCount; }
    uint32_t ackedCount() { return m_ackedCount; }
    uint32_t failedCount() { return m_failedCount; }
    uint32_t retryCount() { return m_retryCount; }

private:
    static EspNowSender* s_instance;

    static void onDataSent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status);
    static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);

    void handleSendStatus(esp_now_send_status_t status);
    void handleReceive(const uint8_t* data, int len);

    bool startNextSend();
    bool sendCurrent();
    void failCurrent();

private:
    uint8_t m_loggerMac[6] = {0};
    bool m_initialized = false;

    static constexpr size_t QUEUE_SIZE = 8;
    PendingLog m_queue[QUEUE_SIZE];
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_count = 0;

    bool m_hasInFlight = false;
    PendingLog m_inFlight = {};
    uint8_t m_retryAttempt = 0;
    uint32_t m_lastSendMs = 0;
    bool m_waitingForAck = false;

    uint32_t m_nextSequence = 1;

    volatile bool m_lastSendCompleted = false;
    volatile bool m_lastSendOk = false;

    uint32_t m_sentCount = 0;
    uint32_t m_ackedCount = 0;
    uint32_t m_failedCount = 0;
    uint32_t m_retryCount = 0;
};