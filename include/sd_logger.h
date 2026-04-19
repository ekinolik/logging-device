#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "app_config.h"

class SDLogger {
    public:
    struct LogEntry {
        String deviceId;
        String line;  // Full ndjson line, no trailing newline required
        uint32_t timestampMs; // used for file naming
    };

    explicit SDLogger(SPIClass& spi);

    bool begin();
    void update();

    bool isInitialized() const;
    bool isAcceptingEntries() const;
    bool isIdle() const;

    void beginShutdown();

    bool enqueue(const LogEntry& entry);
    bool appendNow(const LogEntry& entry); // useful during early bring up

    private:
    SPIClass& m_spi;
    bool m_initialized = false;
    bool m_acceptingEntries = false;
    bool m_shutdownRequested = false;

    LogEntry m_queue[AppConfig::SD_MAX_QUEUE_SIZE];
    size_t m_queueHead = 0;
    size_t m_queueTail = 0;
    size_t m_queueCount = 0;

    bool initSDCard();
    bool dequeue(LogEntry& out);
    bool appendEntryToFile(const LogEntry& entry);
    String buildPath(const String& deviceId, uint64_t timestampMs) const;
    bool ensureDirectoryExists(const String& path);
};
