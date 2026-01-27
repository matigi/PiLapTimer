# PiLapTimer – Development Milestones

This document defines the incremental development milestones for the PiLapTimer project.
Each milestone must be completed, tested, and merged into `main` before proceeding to the next.

Milestones are intentionally small and hardware-verifiable.

Reference:
- docs/gokart-lap-timer-functional-spec.md

---

## Milestone 0 – Project Scaffold & Toolchain

### Objective
Establish a working development environment and flashing workflow.

### Scope
- Arduino-Pico toolchain selected and installed
- Project builds and flashes successfully
- Serial output functional

### Requirements
- Board boots without reset loops
- Serial output at 115200 baud

### Definition of Done
- Firmware flashes successfully
- Serial monitor shows:
  BOOT

---

## Milestone 1 – Display Bring-Up

### Objective
Verify display initialization and basic rendering.

### Scope
- Initialize Waveshare 1.64" AMOLED display
- Render static text
- Render a continuously updating counter

### Requirements
- Display initializes reliably on boot
- No touch input required

### Definition of Done
- Screen displays:
  PiLapTimer
  Uptime: X.XXX s
- Counter updates at least once per second
- No flickering or blank screen

---

## Milestone 2 – Touch Input Test

### Objective
Verify touchscreen input and coordinate mapping.

### Scope
- Initialize touch controller
- Detect touch events
- Display touch coordinates

### Requirements
- Single-touch support only
- Raw coordinates acceptable initially

### Definition of Done
- Touching the screen updates displayed X/Y coordinates in real time
- Touch events do not freeze or crash firmware

---

## Milestone 3 – IR Lap Trigger Detection
## hw-m03-rx-touch-ir-ok
**Status: COMPLETE**

### Objective
Implement low-latency lap detection using IR receiver and interrupts.

### Scope
- Configure IR receiver GPIO
- Attach hardware interrupt
- Capture timestamp on trigger

### Requirements
- Interrupt-based detection only
- Timestamp captured immediately on interrupt
- Serial logging only (no UI yet)

### Definition of Done
- Each IR trigger prints:
  IR HIT @ <timestamp> us
- Multiple triggers are detected reliably
- No missed triggers during rapid passes

---

## Milestone 4 – Lap Validation & Debounce Logic

### Objective
Prevent false or duplicate lap detections.

### Scope
- Minimum lap time threshold
- Post-trigger debounce window
- Ignore invalid triggers

### Requirements
- Configurable minimum lap time
- Configurable debounce duration

### Definition of Done
- Duplicate triggers within debounce window are ignored
- Laps shorter than minimum time are rejected
- Valid laps continue to register normally

## hw-m04-session-ui-ok 
**Status: COMPLETE**

- Touch-based Start / Stop session control
- Reliable IR lap detection (GPIO1, 3.3V)
- Hysteresis + persistence (20ms sampling)
- Lap cooldown + minimum lap enforced
- Last lap + best lap tracking
- CSV lap output over Serial
- Verified via bench testing and real beacon exposure

This is the stable Phase 1 baseline.
---

## hw-m06 – Driver Selection + Fixed-Lap Competitive Runs
**Status: IN PROGRESS**

### Objective
Add Phase 2 driver selection, fixed-lap session flow, and finished results display.

### Scope
- Driver selection (1–10) on S1
- Target lap count selection (min 1, max 99) on S1
- S2 Armed → S3 Running → S4 Finished transitions
- Lap beep on each valid lap
- Distinct completion beep on final lap
- Display total, best, and average on results screen
- In-memory storage of last run per driver (no persistence)

### Definition of Done
- Driver and lap count changes only allowed in IDLE
- First beacon starts timing; laps counted until target reached
- Results screen shows total, best, average, and requires DONE to exit
- Touch ignored during RUNNING
- IR detection logic unchanged

### Objective
Implement core system states and transitions.

### Scope
- BOOT
- IDLE
- ARMED
- RUNNING
- SESSION_END

### Requirements
- IR detection only active in RUNNING state
- Physical buttons control state transitions

### Definition of Done
- State transitions follow:
  BOOT → IDLE → ARMED → RUNNING → SESSION_END → IDLE
- Invalid transitions are ignored
- Current state is visible via serial or display

---

## Milestone 6 – Lap Timing Engine

### Objective
Calculate and track lap times accurately.

### Scope
- Lap time calculation
- Best lap tracking
- Lap delta vs best

### Requirements
- Timing resolution ≤ 1 ms
- No cumulative drift across laps

### Definition of Done
- Lap time, best lap, and delta are calculated correctly
- Values match expected timing within ±1 ms

---

## Milestone 7 – Session UI

### Objective
Present live timing data on the display.

### Scope
- Session screen UI
- Display:
  - Current lap number
  - Last lap time
  - Best lap time
  - Delta vs best

### Requirements
- High contrast
- Large, readable text
- Minimal redraw latency

### Definition of Done
- UI updates immediately after each lap
- No UI lag during active session

---

## Milestone 8 – SD Card Initialization & Logging

### Objective
Persist session data to removable storage.

### Scope
- SD card initialization
- FAT32 file system support
- Session folder creation

### Requirements
- Graceful handling of missing SD card
- No crashes on write failure

### Definition of Done
- Session directory created:
  /sessions/session_YYYYMMDD_HHMM/
- laps.csv written successfully

---

## Milestone 9 – IMU Data Logging

### Objective
Capture and store performance data during laps.

### Scope
- Accelerometer initialization
- Buffered IMU sampling
- High-frequency logging during RUNNING

### Requirements
- 100–200 Hz sampling during RUNNING
- Buffered writes to SD

### Definition of Done
- imu.csv populated with timestamped acceleration data
- No dropped samples during active laps

---

## Milestone 10 – Audio & Visual Feedback

### Objective
Provide immediate user feedback.

### Scope
- Buzzer output
- Audible patterns per event

### Requirements
- Non-blocking tone generation

### Definition of Done
- Single beep on lap detection
- Double beep on best lap
- Rising tone on session start
- Long low tone on error

---

## Milestone 11 – Power Safety & Error Handling

### Objective
Ensure robustness during power loss and error conditions.

### Scope
- Brownout detection
- Safe file flush
- Error notifications

### Requirements
- No SD corruption on sudden power loss

### Definition of Done
- Logging halts safely on power drop
- User notified of errors via display/audio

---

## Milestone 12 – Acceptance & Track Validation

### Objective
Validate system against functional specification acceptance criteria.

### Scope
- Full-session testing
- Vibration tolerance
- Timing accuracy validation

### Definition of Done
- Lap timing accuracy within ±1 ms
- No unexpected resets during session
- All session data written correctly
- UI remains responsive throughout session

---
