#include <Arduino.h>
#include <SD.h>

const uint32_t DEFAULT_WINDOW_SIZE = 5;

bool writeChecksum(uint32_t counter, uint32_t now) {
    File f = SD.open("/log.csv", FILE_APPEND);
    if (!f) {
        Serial.println("File open failed!");
        return false;
    }

    f.printf("%d,%d,", counter, now);
    f.println(counter ^ now);

    f.flush();
    f.close();

    Serial.printf("Wrote: %d\n", counter);
    counter++;

    return true;
}

bool verifyLastLine(uint32_t expectedN, uint32_t expectedNow) {
    Serial.println("Starting verify");
    File f = SD.open("/log.csv", FILE_READ);
    if (!f) return false;

    f.seek(f.size() - 50);

    String lastLine = "";
    while (f.available()) {
        lastLine = f.readStringUntil('\n');
    }

    f.close();

    uint32_t checksum = expectedN ^ expectedNow;

    String expected = String(expectedN) + "," +
                      String(expectedNow) + "," +
                      String(checksum);
    lastLine.trim();
    expected.trim();
    Serial.println("Last Line: -" + lastLine + "-");
    Serial.println("Expecting: -" + expected + "-");
    
    return lastLine == expected;
}

bool WriteAndVerify(uint32_t counter) {
    uint32_t now = millis();

    bool ok = writeChecksum(counter, now);
    if (!ok) {
        return false;
    }

    ok = verifyLastLine(counter, now);
    if (!ok) {
        Serial.println("Failed to read last write!");
        Serial.printf("Counter: %d, MS: %d\n", counter, now);
    }
    
    return ok;
}

bool ParseLastLine(const String& line, uint32_t& n, uint32_t& ms, uint32_t& chk) {
    int c1 = line.indexOf(',');
    if (c1 < 0) return false;
    int c2 = line.indexOf(',', c1 + 1);
    if (c1 < 0) return false;
    
    String sN   = line.substring(0, c1);
    String sMs  = line.substring(c1 + 1, c2);
    String sChk = line.substring(c2 + 1);
    sChk.trim();

    n   = (uint32_t) sN.toInt();
    ms  = (uint32_t) sMs.toInt();
    chk = (uint32_t) sChk.toInt();

    return true;
}

bool VerifyChecksum(uint32_t n, uint32_t ms, uint32_t chk) {
    return (n ^ ms) == chk;
}

String GetLastLine(String filename, size_t window, String lastTail) {
    if (window == 0) {
        window = DEFAULT_WINDOW_SIZE;
    }

    if (!SD.exists(filename)) {
        return "";
    }

    File f = SD.open(filename, FILE_READ);
    if (!f) return "";

    size_t sz = f.size();
    if (sz == 0) {
        f.close();
        return "";
    }

    size_t start = (sz > window) ? (sz - window) : 0;
    f.seek(start);

    String tail = "";
    while (f.available() && tail.length() < DEFAULT_WINDOW_SIZE) {
        tail += (char) f.read();
    }

    f.close();

    int end = tail.length();

    // remove trailing whitespace if we're starting from the end of the file.
    if (lastTail.length() == 0) {
        while (end > 0 && (tail[end - 1] == '\0' || tail[end - 1] == '\r' || tail[end -1] == '\n')) {
            end--;
        }
        tail.remove(end);
    }

    int idx = tail.length();

    while (idx >= 0 && tail[idx - 1] != '\0' && tail[idx - 1] != '\r' && tail[idx - 1] != '\n') {
        idx--;
    }

    tail = tail + lastTail;

    if (idx <= 0) {
        return GetLastLine(filename, window + DEFAULT_WINDOW_SIZE, tail);
    }

    String lastLine = tail.substring(idx);

    return lastLine;
}