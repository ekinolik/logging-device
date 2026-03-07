#include "verifySD.h"

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// #define SD_CS   15 // orange
// #define SD_MOSI 20 // yellow
// #define SD_MISO 19 // green
// #define SD_SCK  18 // blue

#define SD_CS   20 // tan
#define SD_MOSI 19 // orange
#define SD_SCK  18 // yellow
#define SD_MISO 15 // green

#define LED_PIN 16 // white
#define BUTTON_PIN 6 // red

SPIClass spi = SPIClass(FSPI);

void detectButtonPress();

void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    delay(5000);

    Serial.println("Starting Setup...");

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    Serial.println("Initializing SD...");
    if (!SD.begin(SD_CS, spi, 8000000)) {
        Serial.println("SD init failed!");
        return;
    }

    Serial.println("SD init OK.");

    File f = SD.open("/log.txt", FILE_APPEND);
    if (!f) {
        Serial.println("Open /log.txt failed!");
        return;
    }

    f.println("Hello world");
    f.flush();
    f.close();
    Serial.println("Wrote 1 line to /log.txt");

    String lastLine = GetLastLine("/log.csv", 0, "");
    uint32_t n, ms, chk;
    bool ok = ParseLastLine(lastLine, n, ms, chk);
    if (!ok) {
        Serial.println("Failed to parse last line: " + lastLine);
        delay(60000);
        return;
    }

    bool checksumOk = VerifyChecksum(n, ms, chk);
    if (!checksumOk) {
        Serial.println("Checksum failed for last line: " + lastLine);
        delay(60000);
        return;
    } 

    Serial.println("Checksum OK for last line: " + lastLine);

    delay(1000);
}

void loop() {
    detectButtonPress();
    static bool noError = true;
    static int32_t n = 0;
    if (!noError) return;


    noError = WriteAndVerify(n);
    n++;


    delay(1000);

}

void detectButtonPress() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Button pressed!");
    } else {
        digitalWrite(LED_PIN, LOW);
    }
}   