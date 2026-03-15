# Logging Device

An ESP32-C6 firmware project that combines hardware management (buttons, LEDs, power) with SD card data logging and checksum verification.

## Table of Contents

- [Overview](#overview)
- [Hardware Requirements](#hardware-requirements)
- [Getting Started](#getting-started)
- [Architecture](#architecture)
- [Modules](#modules)
  - [AppConfig](#appconfig)
  - [BootState](#bootstate)
  - [ButtonManager](#buttonmanager)
  - [LedManager](#ledmanager)
  - [PowerManager](#powermanager)
  - [VerifySD](#verifysd)
- [Configuration](#configuration)

---

## Overview

The logging device firmware runs on an Espressif ESP32-C6 microcontroller. It:

- Tracks boot history and shutdown reasons across deep-sleep cycles using RTC memory
- Manages a status LED with multiple blink patterns for visual feedback
- Handles button input with debouncing and edge detection
- Implements a multi-stage power-off sequence triggered by a button hold
- Logs data to an SD card in CSV format with XOR checksum verification

---

## Hardware Requirements

| Component | GPIO Pin | Wire Color (reference) |
|-----------|----------|------------------------|
| Status LED | GPIO 16 | White |
| Power Button | GPIO 6 | Red |
| SD Card CS | GPIO 20 | Tan |
| SD Card MOSI | GPIO 19 | Orange |
| SD Card SCK | GPIO 18 | Yellow |
| SD Card MISO | GPIO 15 | Green |

**Microcontroller:** Espressif ESP32-C6-DevKitM-1 (or compatible ESP32-C6 board)

---

## Getting Started

### Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) or VS Code with the [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
- ESP32-C6 development board
- USB cable for flashing and serial monitor

### Build

```bash
platformio run -e esp32-c6
```

### Flash

```bash
platformio run -e esp32-c6 --target upload
```

### Monitor Serial Output

```bash
platformio device monitor -e esp32-c6 -b 115200
```

### Expected Boot Output

```
[BOOT] Boot count: 1
[BOOT] Wake cause: UNDEFINED
[BOOT] Last shutdown reason: NONE
[BOOT] Reset reason: 1
Boot Complete
Starting Setup...
Initializing SD...
SD init OK.
Wrote 1 line to /log.txt
Checksum OK for last line: 42,1234567890,1234567848
```

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  main.cpp (Entry Point)              │
│           Initialization & main event loop           │
└───────────────────────┬─────────────────────────────┘
                        │
         ┌──────────────┼──────────────┬──────────────┐
         ▼              ▼              ▼               ▼
  ┌─────────────┐ ┌──────────┐ ┌────────────┐ ┌───────────┐
  │ButtonManager│ │LedManager│ │PowerManager│ │ BootState │
  │             │ │          │ │            │ │           │
  │ - debounce  │ │ - blink  │ │ - FSM      │ │ - RTC mem │
  │ - edge det. │ │   timer  │ │ - deep     │ │ - shutdown│
  │ - GPIO read │ │ - 4 modes│ │   sleep    │ │   reason  │
  └─────────────┘ └──────────┘ └────────────┘ └───────────┘

┌─────────────────────────────────────────────────────┐
│                     VerifySD                         │
│       SD card logging with checksum verification     │
└──────────────────────────┬──────────────────────────┘
                           ▼
                  ┌─────────────────┐
                  │    SD Card      │
                  │  /log.csv       │
                  │  /log.txt       │
                  └─────────────────┘
```

All hardware constants (GPIO pins, timing values) are centralised in [`AppConfig`](#appconfig).

---

## Modules

### AppConfig

**File:** `include/app_config.h`

A header-only namespace that holds all hardware pin assignments and timing constants used across the project. It is the single source of truth for tunable parameters.

```cpp
namespace AppConfig {
    // Hardware pins
    static constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_6;
    static constexpr gpio_num_t LED_PIN    = GPIO_NUM_16;

    // Button behaviour
    static constexpr bool     BUTTON_ACTIVE_LOW  = true;
    static constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

    // Power management
    static constexpr uint32_t SHUTDOWN_HOLD_MS          = 3000; // ms to hold for shutdown
    static constexpr uint32_t STABLE_SHUTDOWN_RELEASE_MS = 150; // debounce before sleep

    // LED timing
    static constexpr uint32_t LED_FAST_BLINK_MS = 120;
    static constexpr uint32_t LED_SLOW_BLINK_MS = 500;
}
```

---

### BootState

**Files:** `include/boot_state.h` · `src/boot_state.cpp`

Tracks boot history and shutdown reasons using RTC (Real-Time Clock) memory, which persists across deep-sleep wake cycles. A magic-number check (`0xDEADBEEF`) detects fresh power-on versus wake-from-sleep.

#### API

```cpp
class BootState {
public:
    // Must be called once at startup
    static void begin();

    // Record why the device is shutting down (persists in RTC memory)
    static void setShutdownReason(ShutdownReason reason);
    static ShutdownReason getShutdownReason();

    // Number of times the device has booted (incremented by begin())
    static uint32_t getBootCount();

    // ESP-IDF wake cause (e.g. GPIO, timer, undefined)
    static esp_sleep_wakeup_cause_t getWakeupCause();

    // Print a human-readable summary to any Stream (e.g. Serial)
    static void printBootSummary(Stream& out);
};

enum class ShutdownReason {
    NONE             = 0,
    BUTTON_HOLD      = 1,
    OTA              = 2,
    LOW_BATTERY      = 3,
    SOFTWARE_REQUEST = 4
};
```

#### Usage

```cpp
BootState::begin();                   // Initialize / increment boot counter
BootState::printBootSummary(Serial);  // Log boot info over serial

// Before entering deep sleep:
BootState::setShutdownReason(ShutdownReason::BUTTON_HOLD);
```

---

### ButtonManager

**Files:** `include/button_manager.h` · `src/button_manager.cpp`

Provides hardware-independent button input handling with configurable debouncing and one-shot edge detection. Designed for polling inside the main loop.

#### API

```cpp
class ButtonManager {
public:
    // pin       – GPIO pin number
    // activeLow – true if pressing the button pulls the pin LOW
    // debounceMs – minimum stable time before a state change is accepted
    ButtonManager(uint8_t pin, bool activeLow, uint32_t debounceMs);

    void begin();    // Configure GPIO pin mode
    void update();   // Must be called every loop iteration

    bool isPressed() const;      // Current stable state
    bool wasPressedEvent();      // True once after a press edge; clears on read
    bool wasReleasedEvent();     // True once after a release edge; clears on read
};
```

#### Usage

```cpp
ButtonManager button(
    static_cast<uint8_t>(AppConfig::BUTTON_PIN),
    AppConfig::BUTTON_ACTIVE_LOW,
    AppConfig::BUTTON_DEBOUNCE_MS
);
button.begin();

void loop() {
    button.update();
    if (button.wasPressedEvent()) {
        // handle press
    }
}
```

---

### LedManager

**Files:** `include/led_manager.h` · `src/led_manager.cpp`

Controls a GPIO-connected LED with four operating modes. Blinking is timer-based so the main loop never needs to block.

#### API

```cpp
enum class LedMode {
    OFF,         // LED always off
    SOLID_ON,    // LED always on
    FAST_BLINK,  // Toggle every LED_FAST_BLINK_MS (default 120 ms)
    SLOW_BLINK   // Toggle every LED_SLOW_BLINK_MS  (default 500 ms)
};

class LedManager {
public:
    explicit LedManager(uint8_t pin);

    void begin();               // Configure GPIO as output
    void setMode(LedMode mode); // Change the current mode
    LedMode getMode() const;

    void update();              // Must be called every loop iteration
};
```

#### Usage

```cpp
LedManager led(static_cast<uint8_t>(AppConfig::LED_PIN));
led.begin();
led.setMode(LedMode::SOLID_ON);

void loop() {
    led.update();  // Drives the blink timer
}
```

---

### PowerManager

**Files:** `include/power_manager.h` · `src/power_manager.cpp`

Implements a five-state finite state machine (FSM) that translates a button hold into a graceful shutdown sequence with LED feedback, ultimately entering ESP32 deep sleep.

#### Power States

| State | Description | LED |
|-------|-------------|-----|
| `ON` | Normal operation, accepting requests | Solid on |
| `PREPARING_SHUTDOWN` | Button held, waiting for hold threshold | Fast blink |
| `READY_FOR_SHUTDOWN` | Hold time exceeded, waiting for button release | Slow blink |
| `SHUTTING_DOWN` | Transition: waiting for button stability | Off |
| `OFF` | Deep sleep; GPIO wake on button press | — |

#### API

```cpp
class PowerManager {
public:
    // button         – ButtonManager instance to monitor
    // led            – LedManager instance for visual feedback
    // shutdownHoldMs – how long (ms) button must be held to trigger shutdown
    PowerManager(ButtonManager& button, LedManager& led, uint32_t shutdownHoldMs);

    void begin();   // Set initial state (ON, LED solid)
    void update();  // Must be called every loop iteration

    PowerState getState() const;
    bool isAcceptingRequests() const; // False during shutdown sequence
};
```

#### Usage

```cpp
PowerManager powerManager(button, led, AppConfig::SHUTDOWN_HOLD_MS);
powerManager.begin();

void loop() {
    button.update();
    powerManager.update();
    led.update();

    if (powerManager.isAcceptingRequests()) {
        // perform application work here
    }
}
```

---

### VerifySD

**Files:** `src/verifySD.h` · `src/verifySD.cpp`

Provides SD card logging utilities with XOR checksum verification to detect data corruption. Log entries are written in a simple CSV format.

#### CSV Format

```
<counter>,<milliseconds>,<xor_checksum>
```

Example: `42,1234567890,1234567848`

The checksum is `counter XOR milliseconds`.

#### API

```cpp
// Write counter + timestamp + checksum to /log.csv, then read it back
// Returns true if the written data can be read back and verified
bool WriteAndVerify(uint32_t counter);

// Parse a single CSV log line into its three fields
// Returns false if the line does not match the expected format
bool ParseLastLine(const String& line, uint32_t& n, uint32_t& ms, uint32_t& chk);

// Verify that chk == (n XOR ms)
bool VerifyChecksum(uint32_t n, uint32_t ms, uint32_t chk);

// Read the last line from a file using a sliding window
// window   – size of read buffer (0 = use default 5 bytes)
// lastTail – used internally for recursion; pass "" from call sites
String GetLastLine(String filename, size_t window, String lastTail);
```

#### Usage

```cpp
// Write and immediately verify
bool ok = WriteAndVerify(counter);

// Parse and verify the last persisted entry at startup
String lastLine = GetLastLine("/log.csv", 0, "");
uint32_t n, ms, chk;
if (ParseLastLine(lastLine, n, ms, chk)) {
    bool valid = VerifyChecksum(n, ms, chk);
}
```

---

## Configuration

All tunable parameters live in `include/app_config.h` inside the `AppConfig` namespace. Edit that file to change GPIO assignments or timing values without touching module source files.

| Constant | Default | Description |
|----------|---------|-------------|
| `BUTTON_PIN` | `GPIO_NUM_6` | Power button GPIO pin |
| `LED_PIN` | `GPIO_NUM_16` | Status LED GPIO pin |
| `BUTTON_ACTIVE_LOW` | `true` | Button pressed = pin LOW |
| `BUTTON_DEBOUNCE_MS` | `30` | Debounce window in milliseconds |
| `SHUTDOWN_HOLD_MS` | `3000` | Hold duration (ms) required to trigger shutdown |
| `STABLE_SHUTDOWN_RELEASE_MS` | `150` | Stability window (ms) before entering deep sleep |
| `LED_FAST_BLINK_MS` | `120` | Fast-blink toggle period in milliseconds |
| `LED_SLOW_BLINK_MS` | `500` | Slow-blink toggle period in milliseconds |
