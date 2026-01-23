# Go-Kart Lap Timer & Performance Logger
Functional Specification v1.0

---

## 1. Purpose and Scope

This document defines the functional specification for a single-kart lap timing and performance logging system intended for recreational and track-day go-kart use.

The system provides:
- Accurate lap timing
- On-kart real-time feedback
- Session-based data logging for post-run analysis

The system is fully self-contained on the kart, with a fixed start/finish beacon. This document covers Version 1.0 functionality only.

---

## 2. Design Goals

- Sub-millisecond lap timing accuracy
- No dependency on GPS for lap detection
- Single kart operation
- Reliable operation under vibration
- Glove-friendly controls
- Offline operation
- Expandable architecture

---

## 3. System Overview

### 3.1 System Components

- Kart-mounted timing and logging unit
- Fixed start/finish infrared beacon
- Removable storage (TF / microSD card)
- Battery-powered operation
- Touchscreen user interface
- Physical control buttons
- Audible feedback via buzzer

---

## 4. Hardware Architecture

### 4.1 Kart-Mounted Unit

Core hardware:
- RP2350-based microcontroller
- Integrated touchscreen display
- Onboard accelerometer (IMU)
- TF (microSD) card slot

External inputs:
- Infrared receiver module for lap detection
- Physical pushbuttons (minimum two)

Outputs:
- Touchscreen display
- Buzzer
- Optional status LED

---

### 4.2 Start/Finish Line Beacon

Function:
- Continuously transmits a modulated infrared signal

Characteristics:
- Fixed position at start/finish line
- Battery powered
- Weather-resistant enclosure
- Narrow beam pattern for reliable triggering

---

## 5. Lap Detection System

### 5.1 Detection Method

- Infrared beacon emits modulated IR signal
- Kart-mounted IR receiver detects signal
- Detection handled via hardware interrupt
- Timestamp captured immediately upon interrupt

---

### 5.2 Lap Validation Logic

- Minimum lap time threshold enforced
- Lap detection ignored until system is armed
- Debounce window applied after each detection

---

## 6. Timing and Accuracy

Requirements:
- Timing resolution: 1 millisecond or better
- Lap trigger latency: less than 2 milliseconds
- Time source: microcontroller hardware timer
- No accumulated drift between laps

---

## 7. Performance Data Logging

### 7.1 Sensor Data

The onboard accelerometer is used to record:
- Longitudinal acceleration
- Lateral acceleration
- Vertical acceleration

Gyroscope data is optional and not required for v1.0.

---

### 7.2 Logging Frequency

- Idle state: approximately 10 Hz
- Active lap: 100 to 200 Hz
- Post-lap: buffered data flushed to storage

---

## 8. Data Storage

### 8.1 Storage Medium

- TF (microSD) card
- FAT32 file system

---

### 8.2 File Structure

/sessions/
session_YYYYMMDD_HHMM/
laps.csv
imu.csv
summary.json

---

### 8.3 File Definitions

laps.csv:
lap_number,lap_time_ms,delta_best_ms

imu.csv:

summary.json:
{
"total_laps": 0,
"best_lap_ms": 0,
"session_duration_s": 0
}

---

## 9. User Interface

### 9.1 Display Screens

- Home screen: system status and battery level
- Session screen: current lap, last lap, best lap
- Live delta screen: real-time lap delta
- Summary screen: session overview
- Settings screen: configuration and calibration

---

### 9.2 Controls

- Button 1: Start and stop session
- Button 2: Reset or start new session
- Touch input: menu navigation and settings

---

## 10. Audio and Visual Feedback

- Lap detected: single short beep
- Best lap: double beep
- Session start: rising tone
- Error condition: long low tone

---

## 11. Power System

### 11.1 Power Requirements

- Input voltage range: 6 to 12 volts
- Regulated voltage: 5 V and 3.3 V rails
- Average current draw: less than 500 mA
- Peak current draw: less than 1 A

---

### 11.2 Power Protection

- Inline fuse
- Reverse polarity protection
- Brownout detection
- Capacitive hold-up for safe file writes

---

## 12. System States

Defined system states:
- BOOT
- IDLE
- ARMED
- RUNNING
- SESSION_END

State flow:
BOOT -> IDLE -> ARMED -> RUNNING -> SESSION_END -> IDLE

---

## 13. Error Handling

- Missing TF card: logging disabled, user notified
- Write failure: retry, then alert user
- IR signal noise: invalid triggers ignored
- Power loss: attempt graceful shutdown

---

## 14. Environmental Requirements

- Operating temperature: -10 C to +50 C
- Tolerant of kart-level vibration
- Splash-resistant enclosure required

---

## 15. Expandability

The following features are explicitly excluded from version 1.0:
- GPS-based lap detection
- Bluetooth or wireless sync
- Multi-kart support
- Cloud analytics
- Wheel speed sensors

---

## 16. Acceptance Criteria

The system is considered complete when:
- Lap times are detected within plus or minus 1 millisecond
- All session data is written successfully to storage
- User interface remains responsive under vibration
- No unexpected resets occur during a full kart session

---

## 17. Open Questions

- GPS integration approach for future versions
- Wireless data export strategy
- Advanced performance metrics definition
- Session comparison methodology

---

## 18. Revision History

- Version 1.0
- Date: 2026-01-13
- Notes: Initial functional specification


