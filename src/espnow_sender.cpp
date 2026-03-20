#include "espnow_sender.h"

EspNowSender* EspNowSender::s_instance = nullptr;

EspNowSender::EspNowSender() {}

bool EspNowSender::begin(const uint8_t* loggerMac[6]) {
    memcpy(m_loggerMac, loggerMac, 6);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (esp_wifi_set_channel(AppConfig::ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        Serial.println("[ESPNOW-SENDER] Failed to set WiFi channel");
        return false;
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW-SENDER] esp_now_init failed");
        return false;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, m_loggerMac, 6);
    peer.channel = AppConfig::ESPNOW_CHANNEL;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESPNOW-SENDER] Failed to add peer");
        return false;
    }

    s_instance = this;
    esp_now_register_send_cb(&EspNowSender::onDataSent);
    esp_now_register_recv_cb(&EspNowSender::onDataRecv);

    m_initialized = true;
    return true;
}

bool EspNowSender::enqueueLog(const char* deviceID, uint32_t timestampMs, const char* jsonLine) {
    if (m_count >= QUEUE_SIZE) {
        return false;
    }

    PendingLog& slot = m_queue[m_tail];
    memset(&slot, 0, sizeof(slot));

    slot.msg.version = LOG_PKT_VERSION;
    slot.msg.msgType = static_cast<uint8_t>(LogMsgType::APPEND_LOG);
    slot.msg.sequence = m_nextSequence++;
    slot.msg.timestampMs = timestampMs;
    strncpy(slot.msg.deviceID, deviceID, ESPNOW_DEVICE_ID_MAX - 1);
    strncpy(slot.msg.jsonLine, jsonLine, ESPNOW_JSON_MAX_SIZE - 1);

    m_tail = (m_tail + 1) % QUEUE_SIZE;
    m_count++;

    return true;
}

bool EspNowSender::startNextSend() {
    if (m_count == 0) {
        return false;
    }

    m_inFlight = m_queue[m_head];
    m_head = (m_head + 1) % QUEUE_SIZE;
    m_count--;

    m_hasInFlight = true;
    m_retryAttempt = 0;
    
    return sendCurrent();
}

bool EspNowSender::sendCurrent() {
    esp_err_t err = esp_now_send(
        m_loggerMac,
        reinterpret_cast<const uint8_t*>(&m_inFlight.msg),
        sizeof(m_inFlight.msg)
    );

    if (err != ESP_OK) {
        Serial.printf("[ESPNOW-SENDER] esp_now_send failed: %d\n", (int)err);
        if (m_retryAttempt < AppConfig::ESPNOW_MAX_RETRIES) {
            m_retryAttempt++;
            m_retryCount++;
            m_lastSendMs = millis();
            m_waitingForAck = true;
            return false;
        }

        failCurrent();
        return false;
    }

    m_sentCount++;
    m_lastSendMs = millis();
    m_waitingForAck = true;

    Serial.printf("[ESPNOW-SENDER] seq=%lu try=%u\n",
        (unsigned long) m_inFlight.msg.sequence,
        (unsigned)(m_retryAttempt + 1)
    );
    return true;
}

void EspNowSender::failCurrent() {
    m_failedCount++;
    m_hasInFlight = false;
    m_waitingForAck = false;
    memset(&m_inFlight, 0, sizeof(m_inFlight));
}

void EspNowSender::onDataSent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
    if (s_instance) {
        s_instance->handleSendStatus(status);
    }
}

void EspNowSender::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (s_instance && data && len > 0) {
        s_instance->handleReceive(data, len);
    }
}

void EspNowSender::handleSendStatus(esp_now_send_status_t status) {
    // Keep this lightweight
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESPNOW-SENDER] MAC-layer send reported failure");
    }
}

void EspNowSender::handleReceive(const uint8_t* data, int len) {
    if (len != sizeof(AckMessageV1)) {
        return;
    }

    AckMessageV1 ack;
    memcpy(&ack, data, sizeof(ack));

    if (ack.version != LOG_PKT_VERSION || ack.msgType != static_cast<uint8_t>(LogMsgType::ACK)) {
        return;
    }

    if (!m_hasInFlight) {
        return;
    }

    if (ack.sequence != m_inFlight.msg.sequence) {
        return;
    }

    if (strncmp(ack.deviceID, m_inFlight.msg.deviceID, ESPNOW_DEVICE_ID_MAX) != 0) {
        return;
    }

    if (ack.accepted) {
        Serial.printf("[ESPNOW-SENDER] ACK seq=%ly\n", (unsigned long)ack.sequence);
        m_ackedCount++;
        m_hasInFlight = false;
        m_waitingForAck = false;
        memset(&m_inFlight, 0, sizeof(m_inFlight));
    } else {
        Serial.printf("[ESPNOW-SENDER] NACK seq=%lu\n", (unsigned long)ack.sequence);
        // leave in-flight active; timeout/retry logic will resend
    }
}