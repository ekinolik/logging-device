#include "verifySD.h"
#include  "app_config.h"
#include "button_manager.h"
#include "led_manager.h"
#include "power_manager.h"
#include "boot_state.h"

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

#define LOCAL_LED_PIN 16 // white
#define LOCAL_BUTTON_PIN 6 // red

ButtonManager button(
    static_cast<uint8_t>(AppConfig::BUTTON_PIN),
    AppConfig::BUTTON_ACTIVE_LOW,
    AppConfig::BUTTON_DEBOUNCE_MS
);

LedManager led(static_cast<uint8_t>(AppConfig::LED_PIN));

PowerManager powerManager(
    button,
    led,
    AppConfig::SHUTDOWN_HOLD_MS
);

SPIClass spi = SPIClass(FSPI);

void setup() {
    Serial.begin(115200);
    delay(2000);

    BootState::begin();
    BootState::printBootSummary(Serial);

    //gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);

    button.begin();
    led.begin();
    led.setMode(LedMode::SOLID_ON);
    powerManager.begin();

    Serial.println("Boot Complete");

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
    button.update();
    powerManager.update();
    led.update();

    /*
    static bool noError = true;
    static int32_t n = 0;
    if (!noError) return;


    noError = WriteAndVerify(n);
    n++;
    */


    delay(1);
    //delay(1000);

}