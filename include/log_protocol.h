#pragma once

#include <Arduino.h>

// Protocol versioning.
static constexpr uint8_t LOG_PKT_VERSION = 1;

// Sizing.
static constexpr size_t ESPNOW_DEVICE_ID_MAX = 16;
static constexpr size_t ESPNOW_JSON_MAX_SIZE = 180;

// Message types.
enum class LogMsgType : uint8_t {
    APPEND_LOG = 1,
    ACK = 2,
};

// Packet format.
struct LogMessageV1 {
    uint8_t version;
    uint8_t msgType;
    uint32_t sequence;
    uint32_t timestampMs;
    char deviceID[ESPNOW_DEVICE_ID_MAX];
    char jsonLine[ESPNOW_JSON_MAX_SIZE];
};

// Ack format
struct AckMessageV1 {
    uint8_t version;
    uint8_t msgType;
    uint32_t sequence;
    uint8_t accepted; // 1 = logger accepted into queue, 0 = rejected
    char deviceID[ESPNOW_DEVICE_ID_MAX];
};