# PiLapTimer – Hardware Platform & Libraries

This document defines the hardware platform, supported libraries, and low-level constraints for the PiLapTimer project.

This document is **authoritative** for all hardware interaction and must be reviewed before any firmware development.

Reference:
- Waveshare RP2350 Touch AMOLED 1.64" Wiki  
  https://www.waveshare.com/wiki/RP2350-Touch-AMOLED-1.64

---

## 1. Board Overview

### Board Name
**Waveshare RP2350 Touch AMOLED 1.64"**

### MCU
- RP2350 (Raspberry Pi Pico–compatible)
- Dual-core ARM Cortex-M
- Hardware timers available
- GPIO interrupts supported
- 3.3 V logic

---

## 2. Integrated Peripherals

### 2.1 Display

- Type: 1.64" AMOLED
- Resolution: 280 × 456
- Interface: SPI
- Color depth: 16-bit (RGB565)
- High refresh rate supported

#### Display Notes
- Display uses SPI and must be initialized early
- Framebuffer updates should be optimized to avoid blocking timing logic
- Partial redraws preferred over full refresh

---

### 2.2 Touchscreen

- Type: Capacitive touch
- Interface: I2C
- Single-touch supported
- Coordinate orientation may not match display orientation by default

#### Touch Notes
- Touch polling must be non-blocking
- Coordinate mapping handled in firmware
- Touch disabled during timing-critical operations if necessary

---

### 2.3 IMU (Onboard)

- Accelerometer present (model per Waveshare documentation)
- Interface: I2C
- Used for longitudinal, lateral, and vertical acceleration logging
- Gyroscope optional and not required for v1.0

---

### 2.4 microSD Card Slot

- Interface: SPI
- FAT32 filesystem
- Used for session-based logging

#### SD Notes
- SD card shares SPI bus with display
- Chip Select lines must be managed carefully
- All writes must be buffered and flushed safely

---

## 3. External Interfaces

### 3.1 Infrared Receiver (Lap Detection)

- External IR receiver module
- Digital output
- Connected to GPIO with interrupt capability

#### IR Requirements
- Interrupt-based detection only
- Timestamp captured immediately in ISR
- No decoding performed in interrupt handler

---

### 3.2 Physical Buttons

- Minimum two momentary buttons
- GPIO input with pull-ups
- Used for:
  - Session start/stop
  - Reset / new session

---

### 3.3 Buzzer

- GPIO-driven piezo buzzer
- Non-blocking tone generation required
- Used for audible feedback events

---

## 4. Power Characteristics

- Input voltage range: 6–12 V (external regulation required)
- Onboard regulation to 5 V / 3.3 V
- Brownout detection available
- Sudden power loss must not corrupt SD card

---

## 5. Software Environment

### 5.1 Toolchain

- Arduino-Pico core
- Arduino IDE or Arduino CLI
- C++ (Arduino framework)

---

### 5.2 Required Libraries

The following libraries are expected to be used unless otherwise justified:

#### Display
- Waveshare-provided AMOLED library **or**
- Adafruit_GFX-compatible driver (SPI)

#### Touch
- I2C touch controller library as specified by Waveshare
- Single-touch only

#### SD Card
- Arduino SD or SdFat library
- FAT32 support required

#### IMU
- Vendor-provided IMU driver or Arduino-compatible equivalent

---

## 6. Timing & Interrupt Constraints

- Lap detection must use GPIO hardware interrupt
- ISR must:
  - capture timestamp
  - set a flag
  - exit immediately
- No SD access, display updates, or logging in ISR
- Timing resolution target: ≤ 1 ms

---

## 7. Known Constraints & Rules

- SPI bus is shared between display and SD card
- Display refresh must not block lap timing
- No dynamic memory allocation in time-critical paths
- UI must defer to timing and logging logic

---

## 8. Future Platform Considerations (Not v1.0)

- GPS module support
- Wireless connectivity
- Additional sensors
- External CAN or serial interfaces

### Arduino build note (Waveshare fonts)
Some Waveshare font definitions (e.g. Font16/Font20) must be located in the sketch folder
(`firmware/pilaptimer/`) so Arduino compiles and links them. Keeping these font sources in
subfolders may cause undefined reference errors.

These features are explicitly excluded from Version 1.0.

---
