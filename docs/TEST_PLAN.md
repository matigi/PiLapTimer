# PiLapTimer – Test Plan

This document defines the verification and validation test plan for the PiLapTimer project.
All tests are derived from the functional specification and development milestones.

Reference documents:
- docs/gokart-lap-timer-functional-spec.md
- docs/MILESTONES.md
- docs/STATE_MACHINE.md

---

## 1. Test Philosophy

The system is tested incrementally using:
- Bench tests (no kart, no vibration)
- Functional simulation (manual triggers)
- Real-world validation (track testing)

Each milestone must pass all relevant tests before merging to `main`.

---

## 2. Test Environment

### 2.1 Hardware
- RP2350-based Waveshare 1.64" Touch AMOLED board
- IR receiver module
- Momentary pushbuttons (minimum two)
- microSD card (FAT32)
- Buzzer
- Stable bench power supply (or battery)

### 2.2 Software
- Arduino-Pico toolchain
- Serial monitor @ 115200 baud
- Firmware built from `main` branch only

---

## 3. Test Levels

| Level | Description |
|------|-------------|
| Unit | Single component verification |
| Integration | Multiple subsystems interacting |
| System | Full end-to-end behavior |
| Acceptance | Real-world validation |

---

## 4. Milestone-Based Test Cases

---

## 4.1 Milestone 0 – Project Scaffold

### Test 0.1 – Firmware Flash
**Type:** Unit  
**Procedure:**
1. Flash firmware to board
2. Open serial monitor

**Expected Result:**
- Board boots without reset loop
- Serial prints `BOOT`

**Pass Criteria:**
- BOOT message appears within 2 seconds of power-on

---

## 4.2 Milestone 1 – Display Bring-Up

### Test 1.1 – Display Initialization
**Type:** Unit  
**Procedure:**
1. Power cycle board
2. Observe display

**Expected Result:**
- Display initializes reliably
- No flicker or blank screen

**Pass Criteria:**
- Text visible within 1 second of boot

---

### Test 1.2 – Uptime Counter
**Type:** Unit  
**Procedure:**
1. Observe uptime value for 10 seconds

**Expected Result:**
- Counter increases monotonically

**Pass Criteria:**
- Counter increments at least once per second
- No resets or freezes

---

## 4.3 Milestone 2 – Touch Input

### Test 2.1 – Touch Detection
**Type:** Unit  
**Procedure:**
1. Touch various points on screen

**Expected Result:**
- Touch coordinates update live

**Pass Criteria:**
- Touch response latency < 100 ms
- No firmware crashes

---

### Test 2.2 – Touch Stability
**Type:** Integration  
**Procedure:**
1. Hold finger on screen
2. Drag finger slowly

**Expected Result:**
- Coordinates update smoothly

**Pass Criteria:**
- No missed touch events
- No jitter-induced lockups

---

## 4.4 Milestone 3 – IR Lap Trigger Detection

### Test 3.1 – IR Interrupt Trigger
**Type:** Unit  
**Procedure:**
1. Manually trigger IR receiver
2. Observe serial output

**Expected Result:**
- Timestamp printed immediately

**Pass Criteria:**
- Output:
  IR HIT @ <timestamp> us
- Latency < 2 ms from trigger

---

### Test 3.2 – Rapid Trigger Test
**Type:** Integration  
**Procedure:**
1. Trigger IR multiple times rapidly

**Expected Result:**
- All triggers detected

**Pass Criteria:**
- No missed interrupts
- No system instability

---

## 4.5 Milestone 4 – Lap Validation & Debounce

### Test 4.1 – Debounce Enforcement
**Type:** Integration  
**Procedure:**
1. Trigger IR twice within debounce window

**Expected Result:**
- Second trigger ignored

**Pass Criteria:**
- Only one lap registered

---

### Test 4.2 – Minimum Lap Time
**Type:** Integration  
**Procedure:**
1. Trigger IR before minimum lap time elapsed

**Expected Result:**
- Lap rejected

**Pass Criteria:**
- No lap recorded
- Rejection logged

---

## 4.6 Milestone hw-m05 – Known-Good Receiver + Beacon Baseline

### Test 4.6.1 – Bench Line-of-Sight Toggle
**Type:** Integration  
**Procedure:**
1. Power the ESP32-S3 beacon (burst-gated, 38 kHz).
2. With the RP2350 receiver running, cover the IR path between beacon and receiver.
3. Uncover the IR path and repeat several times.

**Expected Result:**
- Serial output shows beacon presence toggling with cover/uncover events.
- No false lap increments while IDLE.

**Pass Criteria:**
- Beacon present state visibly flips in UI and/or serial with each cover/uncover.

### Test 4.6.2 – Lap Trigger While RUNNING
**Type:** Integration  
**Procedure:**
1. Tap START to enter RUNNING.
2. Uncover the beacon path to create a fresh absent→present transition.

**Expected Result:**
- Lap count increments.
- Serial prints a `LAP` line and CSV line.

**Pass Criteria:**
- Lap count increases by 1 per valid transition.

### Test 4.6.3 – Cooldown / Min Lap Ignore
**Type:** Integration  
**Procedure:**
1. While RUNNING, repeatedly toggle the beacon rapidly (faster than cooldown/min lap).

**Expected Result:**
- Serial prints `IGNORE` lines with `(debounce)` or `(minlap)` reasons.

**Pass Criteria:**
- No new lap is recorded until both cooldown and minimum lap time elapse.

---

## 4.6 Milestone 5 – State Machine

### Test 5.1 – State Transition Validity
**Type:** Integration  
**Procedure:**
1. Use buttons to cycle states

**Expected Result:**
- Only valid transitions allowed

**Pass Criteria:**
- State flow matches:
  BOOT → IDLE → ARMED → RUNNING → SESSION_END → IDLE

---

### Test 5.2 – IR Gating by State
**Type:** Integration  
**Procedure:**
1. Trigger IR in each state

**Expected Result:**
- IR only active in RUNNING

**Pass Criteria:**
- No lap recorded outside RUNNING

---

## 4.7 Milestone 6 – Lap Timing Engine

### Test 6.1 – Lap Time Accuracy
**Type:** System  
**Procedure:**
1. Trigger IR at known intervals

**Expected Result:**
- Lap time calculated correctly

**Pass Criteria:**
- Timing error ≤ ±1 ms

---

### Test 6.2 – Best Lap Tracking
**Type:** System  
**Procedure:**
1. Record multiple laps with varying durations

**Expected Result:**
- Best lap updated correctly

**Pass Criteria:**
- Best lap equals minimum lap time

---

## 4.8 Milestone 7 – Session UI

### Test 7.1 – UI Responsiveness
**Type:** System  
**Procedure:**
1. Observe UI during active laps

**Expected Result:**
- UI updates immediately after lap

**Pass Criteria:**
- No visible lag or missed updates

---

## 4.9 Milestone 8 – SD Card Logging

### Test 8.1 – SD Initialization
**Type:** Integration  
**Procedure:**
1. Boot with SD inserted
2. Boot without SD inserted

**Expected Result:**
- Graceful handling of missing SD

**Pass Criteria:**
- No crash
- User notified when SD missing

---

### Test 8.2 – File Creation
**Type:** Integration  
**Procedure:**
1. Start a session
2. End a session
3. Inspect SD card

**Expected Result:**
- Session directory created
- laps.csv populated

**Pass Criteria:**
- Files readable on PC

---

## 4.10 Milestone 9 – IMU Logging

### Test 9.1 – Sampling Rate
**Type:** Integration  
**Procedure:**
1. Run active session
2. Analyze imu.csv timestamps

**Expected Result:**
- 100–200 Hz sampling

**Pass Criteria:**
- No significant gaps (>10 ms)

---

## 4.11 Milestone 10 – Audio Feedback

### Test 10.1 – Audible Patterns
**Type:** Unit  
**Procedure:**
1. Trigger each event type

**Expected Result:**
- Correct tone per event

**Pass Criteria:**
- Patterns match spec

---

## 4.12 Milestone 11 – Power Safety

### Test 11.1 – Power Loss During Logging
**Type:** System  
**Procedure:**
1. Remove power mid-session
2. Inspect SD card

**Expected Result:**
- No corruption

**Pass Criteria:**
- Files readable
- Partial session recoverable

---

## 4.13 Milestone 12 – Acceptance & Track Validation

### Test 12.1 – Full Session Test
**Type:** Acceptance  
**Procedure:**
1. Run full kart session

**Expected Result:**
- Stable operation

**Pass Criteria:**
- No resets
- Accurate lap timing
- Complete session logs

---

## 5. Regression Testing

Any change affecting:
- timing
- interrupts
- state machine
- logging

requires re-running:
- Tests 3.x
- Tests 4.x
- Tests 6.x
- Tests 8.x

---

## 6. Test Completion Criteria

The system is considered validated when:
- All milestone tests pass
- Acceptance tests pass on-track
- Functional specification acceptance criteria are met

---
