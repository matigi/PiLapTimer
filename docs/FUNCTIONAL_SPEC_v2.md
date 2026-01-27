> ⚠️ Status: Historical Reference (Phase 1)
>
> This document describes the original Phase 1 functionality.
> Current development follows FUNCTIONAL_SPEC_v2.md.


# PiLapTimer – Functional Specification v2
## Phase 2: Competitive Time Attack Mode

---

## 1. Purpose & Design Intent

PiLapTimer is a portable lap timing system designed to compare driver skill, encourage friendly competition, and provide clear, repeatable performance metrics.

The system prioritizes:
- Fair comparison between drivers
- Simple, repeatable run structure
- Immediate feedback
- Low cognitive load during use
- Reliability over feature complexity

This is a time-attack / hot-lap system, not wheel-to-wheel race control.

---

## 2. System Overview

### Core Components
- IR Beacon (ESP32-S3)
  - 38 kHz carrier
  - Burst-gated transmission
- Receiver / UI Unit (RP2350)
  - Touchscreen AMOLED display
  - IR receiver (GPIO1, 3.3V)
  - Optional buzzer
- User Interaction
  - Touch buttons
  - Visual feedback
  - Audible feedback (beep)

---

## 3. Operating Modes

Detailed touchscreen layouts and state transitions are defined in:
docs/UI_FLOW_PHASE2.md

### 3.1 Idle Mode
Default state after boot or session end.

Display shows:
- Selected Driver
- Target Lap Count
- Last completed run (if available)

User can:
- Select driver (1–10)
- Select target lap count
- Start a new session

---

### 3.2 Configuration Mode
Prior to starting a run, the user may configure:

#### Driver Selection
- Driver IDs: 1–10
- One active driver at a time
- Driver selected via touch buttons

#### Lap Count Selection
- User selects number of laps (e.g. 3, 5, 10)
- Lap count defines when the run ends

Configuration is locked once a run starts.

---

### 3.3 Armed Mode (Pre-Run)
- System is waiting for the first IR beacon detection
- No timing active yet

Purpose:
- Ensures timing begins fairly at the first crossing
- Avoids “out lap” ambiguity

---

### 3.4 Running Mode
Activated on the first beacon detection.

#### Timing Rules
- First beacon hit:
  - Starts session timer
  - Lap counter = 0
- Each subsequent beacon hit:
  - Increments lap count
  - Records lap time
  - Triggers short audible beep
- IR detection uses:
  - 20 ms sampling window
  - Falling-edge counting
  - Hysteresis and persistence
  - Cooldown and minimum lap enforcement

---

### 3.5 Finished Mode
Triggered automatically when:
laps_completed == target_laps

Behavior:
- Session ends immediately
- Final lap time recorded
- Distinct completion beep emitted
- Results frozen on screen

---

## 4. Session Results

After completion, the display shows:
- Driver ID
- Laps completed
- Total time (from first beacon to final beacon)
- Best lap time
- Average lap time

Example:
Driver 4  
Laps: 5  

Total: 1:23.456  
Best: 0:15.892  
Avg:  0:16.691  

---

## 5. Driver History & Comparison

### 5.1 Per-Driver Run Memory
- The system stores the most recent completed run for each driver (1–10)
- Stored in RAM only (no persistent storage required)

For each driver:
- Total time
- Best lap
- Average lap
- Lap count

---

### 5.2 Comparison Display
When selecting a driver, the UI may display:
- That driver’s previous run
- Or the last completed run by another driver

This enables immediate comparison and competition.

---

## 6. Audible Feedback (Buzzer)

### 6.1 Lap Beeps
- Short beep on each valid lap detection

### 6.2 Completion Beep
- Distinct (longer or double) beep on final lap

### 6.3 Non-Blocking Requirement
- Buzzer control must be non-blocking
- No delay() calls permitted

---

## 7. Data Output (Serial)

### 7.1 CSV Output
Each completed lap outputs one CSV line:

CSV,lap_number,lap_time_ms,session_time_ms,best_lap_ms

This enables:
- Post-session analysis
- Import into spreadsheets or scripts
- Future storage expansion

---

## 8. Reliability Constraints (MANDATORY)

### IR Detection
- IR receiver powered at 3.3V
- OUT connected directly to GPIO1
- GPIO configured as INPUT_PULLUP
- No voltage divider

Known-good thresholds:
- Sample window: 20 ms
- ON ≥ 3 falls
- OFF ≤ 1 fall
- Streak ≥ 2
- Cooldown: 2000 ms
- Minimum lap time: 5000 ms

These values must not be changed unless explicitly requested.

---

## 9. Performance Requirements

- UI updates must not block IR detection
- Loop must remain non-blocking
- IR ISR must remain lightweight
- Touch input debounced in software
- System must tolerate ambient sunlight

---

## 10. Explicit Non-Goals (Phase 2)

Out of scope:
- Wireless communication
- Persistent storage
- Multi-kart synchronization
- Cloud connectivity
- User accounts or authentication

---

## 11. Phase 2 Success Criteria

Phase 2 is complete when:
- Drivers can be selected reliably
- Lap count can be configured
- Runs start and end automatically
- Results are clearly displayed
- Buzzer feedback works consistently
- Driver comparisons are possible
- No regression in IR reliability

---

## 12. Future Expansion (Informational Only)

Potential future phases may include:
- Wireless telemetry
- Persistent driver statistics
- Leaderboards
- Multi-device synchronization

These are not to be implemented during Phase 2.
