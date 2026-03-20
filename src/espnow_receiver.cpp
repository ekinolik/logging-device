#include "espnow_receiver.h"
#include "log_protocol.h"

EspNowReceiver* EspNowReceiver::s_instance = nullptr;

namespace {
    const uint8_t MSG_APPEND_LOG = 1;
}

EspNowReceiver::EspNowReceiver() {}

bool EspNowReceiver::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (esp_wifi_set_channel(AppConfig::ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        Serial.println("[ESPNOW] failed to set channel");
        return false;
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] init failed");
    }

    s_instance = this;

    if (esp_now_register_recv_cb(&EspNowReceiver::onDataRecv) != ESP_OK) {
        Serial.println("[ESPNOW] failed to register recv callback");
        return false;
    };

    m_initialized = true;

    String macStr = WiFi.macAddress();
    Serial.print("[ESPNOW] Ready, MAC=");
    Serial.println(macStr);

    return true;
}

void EspNowReceiver::update() {
    if (!m_initialized) {
        return;
    }

    RxItem item;
    if (!dequeueRx(item)) {
        return;
    }

    processRxItem(item);
}

bool EspNowReceiver::dequeueMessage(ReceivedMessage& out) {
    if (m_msgCount == 0) {
        return false;
    }

    out = m_msgQueue[m_msgHead];
    m_msgHead = (m_msgHead + 1) % AppConfig::ESPNOW_RX_QUEUE_SIZE;
    m_msgCount--;

    return true;
}

// Callback.
void EspNowReceiver::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!s_instance || !info || !data || len <= 0) {
        return;
    }

    s_instance->handleReceive(info, data, len);
}

void EspNowReceiver::handleReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len > 250) {
        m_droppedPackets++;
        return;
    }

    maybeAddPeer(info->src_addr);

    if (!enqueueRx(info->src_addr, data, static_cast<uint8_t>(len))) {
        m_droppedPackets++;
    }
}

// Raw Queue
bool EspNowReceiver::enqueueRx(const uint8_t mac[6], const uint8_t* data, uint8_t len) {
    if (m_rxCount >= AppConfig::ESPNOW_RX_QUEUE_SIZE) {
        return false;
    }

    RxItem& slot = m_rxQueue[m_rxTail];
    
    memcpy(slot.mac, mac, 6);
    memcpy(slot.data, data, len);
    slot.len = len;

    m_rxTail = (m_rxTail + 1) % AppConfig::ESPNOW_RX_QUEUE_SIZE;
    m_rxCount++;

    return true;
}

bool EspNowReceiver::dequeueRx(RxItem& out) {
    if (m_rxCount == 0) {
        return false;
    }

    out = m_rxQueue[m_rxHead];
    m_rxHead = (m_rxHead + 1) % AppConfig::ESPNOW_RX_QUEUE_SIZE;
    m_rxCount--;

    return true;
}

// Parsing
void EspNowReceiver::processRxItem(const RxItem& item) {
    if (item.len != sizeof(LogMessageV1)) {
        m_parseErrors++;
        Serial.printf("[ESPNOW] Bad size: %u\n", item.len);

        return;
    }

    LogMessageV1 msg;
    memcpy(&msg, item.data, sizeof(LogMessageV1));

    if (msg.version != LOG_PKT_VERSION || msg.msgType != MSG_APPEND_LOG) {
        m_parseErrors++;
        Serial.println("[ESPNOW] Invalid version/type");

        return;
    }

    // Safety null-termination
    msg.deviceID[ESPNOW_DEVICE_ID_MAX - 1] = '\0';
    msg.jsonLine[ESPNOW_JSON_MAX_SIZE - 1] = '\0';

    ReceivedMessage out;
    memcpy(out.mac, item.mac, 6);
    out.msg = msg;
    if (!enqueueParsed(out)) {
        m_droppedPackets++;
        return;
    }

    m_acceptedPackets++;
}

// Parsed Queue
bool EspNowReceiver::enqueueParsed(const ReceivedMessage& msg) {
    if (m_msgCount >= AppConfig::ESPNOW_RX_QUEUE_SIZE) {
        return false;
    }

    m_msgQueue[m_msgTail] = msg;
    m_msgTail = (m_msgTail + 1) % AppConfig::ESPNOW_RX_QUEUE_SIZE;
    m_msgCount++;

    return true;
}

// Peer management
void EspNowReceiver::maybeAddPeer(const uint8_t mac[6]) {
    if (AppConfig::ESPNOW_REQUIRE_KNOWN_PEERS) {
        return;
    }

    if (esp_now_is_peer_exist(mac)) {
        return;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = AppConfig::ESPNOW_CHANNEL;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) {
        Serial.printf("[ESPNOW] add_peer failed: %d\n", (int)err);
    }
}

bool EspNowReceiver::sendAck(const uint8_t mac[6], const char* deviceID, const uint32_t sequence, bool accepted) {
    AckMessageV1 ack = {};
    ack.version = LOG_PKT_VERSION;
    ack.msgType = static_cast<uint8_t>(LogMsgType::ACK);
    ack.sequence = sequence;
    ack.accepted = accepted ? 1 : 0;
    strncpy(ack.deviceID, deviceID, ESPNOW_DEVICE_ID_MAX - 1);

    maybeAddPeer(mac);

    esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&ack), sizeof(ack));
    if (err != ESP_OK) {
        Serial.printf("[ESPNOW] sendAck failed err=%d\n", int(err));
        return false;
    }

    return true;
}