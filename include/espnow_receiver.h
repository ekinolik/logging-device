#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "log_protocol.h"
#include "app_config.h"

class EspNowReceiver {
public:
    struct ReceivedMessage {
        uint8_t mac[6];
        LogMessageV1 msg;
        int8_t rssi;
    };

    EspNowReceiver();

    bool begin();
    void update();

    // Consumer pulls parsed messages from here.
    bool dequeueMessage(ReceivedMessage& out);

    bool isInitialized() const { return m_initialized; }
    uint32_t droppedPackets() const { return m_droppedPackets; }
    uint32_t parseErrors() const { return m_parseErrors; }
    uint32_t acceptedPackets() const { return m_acceptedPackets; }
    bool sendAck(const uint8_t mac[6], const char* deviceID, const uint32_t sequence, bool accepted);

private:
    // Raw RX item (from callback)
    struct RxItem {
        uint8_t mac[6];
        uint8_t data[250];
        uint8_t len;
        int8_t rssi;
    };

    static EspNowReceiver* s_instance;

    static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);

    void handleReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    bool enqueueRx(const uint8_t mac[6], const uint8_t* data, uint8_t len, int8_t rssi);
    bool dequeueRx(RxItem& out);
    void processRxItem(const RxItem& item);
    bool enqueueParsed(const ReceivedMessage& msg);
    void maybeAddPeer( const uint8_t mac[6] );

    bool m_initialized = false;

    // Raw RX queue (from ISR).
    RxItem m_rxQueue[AppConfig::ESPNOW_RX_QUEUE_SIZE];
    size_t m_rxHead = 0;
    size_t m_rxTail = 0;
    size_t m_rxCount = 0;

    // Parsed message queue.
    ReceivedMessage m_msgQueue[AppConfig::ESPNOW_RX_QUEUE_SIZE];
    size_t m_msgHead = 0;
    size_t m_msgTail = 0;
    size_t m_msgCount = 0;

    // Stats.
    uint32_t m_droppedPackets = 0;
    uint32_t m_parseErrors = 0;
    uint32_t m_acceptedPackets = 0;
};