#include "sd_logger.h"
#include "app_config.h"

SDLogger::SDLogger(SPIClass& spi) : m_spi(spi) {}

bool SDLogger::begin() {
    m_acceptingEntries = true;
    m_shutdownRequested = false;
    m_queueHead = 0;
    m_queueTail = 0;
    m_queueCount = 0;

    m_initialized = initSDCard();

    return m_initialized;
}

bool SDLogger::initSDCard() {
    pinMode(AppConfig::SD_CS_PIN, OUTPUT);
    digitalWrite(AppConfig::SD_CS_PIN, HIGH);

    m_spi.begin(AppConfig::SD_SCK_PIN, AppConfig::SD_MISO_PIN, AppConfig::SD_MOSI_PIN, AppConfig::SD_CS_PIN);

    Serial.println("[SD] Initializing SD...");
    if (!SD.begin(AppConfig::SD_CS_PIN, m_spi, AppConfig::SD_SPI_FREQUENCY)) {
        Serial.println("[SD] SD init failed!");
        return false;
    }

    Serial.println("[SD] SD init success!");

    return true;
}

bool SDLogger::isInitialized() const {
    return m_initialized;
}

bool SDLogger::isAcceptingEntries() const {
    return m_acceptingEntries;
}

bool SDLogger::isIdle() const {
    return m_queueCount == 0;
}

void SDLogger::beginShutdown() {
    m_acceptingEntries = false;
    m_shutdownRequested = true;
}

bool SDLogger::enqueue(const LogEntry& entry) {
    if (!m_initialized || !m_acceptingEntries) {
        return false;
    }

    if (m_queueCount >= AppConfig::SD_MAX_QUEUE_SIZE) {
        Serial.println("[SD] Queue full, dropping log entry");
        return false;
    }

    m_queue[m_queueTail] = entry;
    m_queueTail = (m_queueTail + 1) % AppConfig::SD_MAX_QUEUE_SIZE;
    m_queueCount++;

    return true;
}

bool SDLogger::dequeue(LogEntry& out) {
    if (m_queueCount == 0) {
        return false;
    }

    out = m_queue[m_queueHead];
    m_queueHead = (m_queueHead + 1) % AppConfig::SD_MAX_QUEUE_SIZE;
    m_queueCount--;

    return true;
}

void SDLogger::update() {
    if (!m_initialized) {
        return;
    }

    LogEntry entry;
    if (!dequeue(entry)) {
        return;
    }

    if (!appendEntryToFile(entry)) {
        Serial.println("[SD] Failed to append log entry");
    }
}

bool SDLogger::appendNow(const LogEntry& entry) {
    if (!m_initialized || !m_acceptingEntries) {
        return false;
    }

    return appendEntryToFile(entry);
}

String SDLogger::buildPath(const String& deviceId, uint64_t timestampMs) const {
    // For phase 2 we keep this simple; use millis() placeholder for date bucket.
    // Later we will use the real RTC/NTP date.
    uint32_t dateBucket = static_cast<uint32_t>(timestampMs / 86400000ULL); // 1 day in ms
    String dir = "/logs/" + deviceId;
    String path = dir + "/day-" + String(dateBucket) + ".ndjson";

    return path;
}

bool SDLogger::ensureDirectoryExists(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash <= 0) {
        return true; // No directory component, use root
    }

    String dir = path.substring(0, lastSlash);
    if (SD.exists(dir)) {
        return true;
    }

    // Create each directory level if needed.  Docs say this is not needed, but in practice it seems to fail if parent directories don't exist.
    // https://docs.arduino.cc/libraries/sd/
    String current = "";
    int start = 1; // Skip leading slash
    while (true) {
        int slash = dir.indexOf('/', start);

        if (slash == -1) {
            current = dir;
        } else {
            current = dir.substring(0, slash);
        }

        if (!SD.exists(current)) {
            Serial.println("[SD] mkdir " + current);
            if (!SD.mkdir(current)) {
                Serial.println("[SD] Failed to create directory " + current);
                return false;
            }
        }

        if (slash == -1) break;
        start = slash + 1;
    }

    return true;
}

bool SDLogger::appendEntryToFile(const LogEntry& entry) {
    const String path = buildPath(entry.deviceId, entry.timestampMs);

    if (!ensureDirectoryExists(path)) {
        Serial.println("[SD] Failed creating directory for " + path);
        return false;
    }

    File f = SD.open(path, FILE_APPEND);
    if (!f) {
        Serial.println("[SD] Open failed " + path);
        return false;
    }

    f.println(entry.line);
    f.flush();
    f.close();

    Serial.println("[SD] Appended " + path);

    return true;
}