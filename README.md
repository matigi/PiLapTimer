# PiLapTimer – Go-Kart Lap Timer & Performance Logger

PiLapTimer is a self-contained, kart-mounted lap timing and performance logging system designed for recreational and track-day go-kart use.

The system uses a fixed infrared (IR) beacon at the start/finish line and a kart-mounted receiver to achieve **sub-millisecond lap timing accuracy without GPS**. All timing, display, and data logging is performed locally on the kart.

This repository contains firmware, documentation, and test plans for **Version 1.0** of the system.

---

## 1. Project Goals

- Sub-millisecond lap timing accuracy
- No GPS dependency
- Reliable operation under vibration
- Glove-friendly controls
- Offline operation
- Session-based data logging
- Expandable architecture for future versions

---

## 2. Hardware Platform

### Kart-Mounted Unit
- Waveshare RP2350-based board
- Integrated 1.64" touch AMOLED display
- Onboard accelerometer (IMU)
- TF (microSD) card slot
- Physical pushbuttons
- Buzzer for audible feedback

### External Components
- Fixed-position IR start/finish beacon
- IR receiver module mounted on kart

---

## 3. Development Environment

### Toolchain
- **Arduino-Pico core** (RP2350 / Pico-compatible)
- Arduino IDE or Arduino CLI
- Serial monitor @ 115200 baud

### Programming Language
- C++ (Arduino framework)

---

## 4. Repository Structure

PiLapTimer/
├── firmware/
│ └── pilaptimer/
│   └── pilaptimer.ino # Main firmware entry point
│ └── ir_beacon/
│   └── ir_beacon.ino # IR start/finish beacon sketch
│
├── docs/
│ ├── gokart-lap-timer-functional-spec.md
│ ├── MILESTONES.md
│ ├── TEST_PLAN.md
│ ├── STATE_MACHINE.md
│ └── WIRING.md
│ └── PLATFORM.md
│ └── IR_BEACON.md
│
├── data/ # Example output files (CSV / JSON)
│
├── README.md
└── .gitignore
---

## 5. Core Documentation (Authoritative)

The following documents define system behavior and **must be treated as authoritative**:

- `docs/gokart-lap-timer-functional-spec.md`  
  → Functional requirements and acceptance criteria

- `docs/MILESTONES.md`  
  → Incremental development plan (work must follow this order)

- `docs/TEST_PLAN.md`  
  → Bench, integration, and acceptance testing

- `docs/STATE_MACHINE.md`  
  → System states and valid transitions

- `docs/PLATFORM.md`  
  → Hardware platform, libraries, and low-level constraints

Any change to behavior **must be reflected in documentation first**.

---

## 6. Development Rules (IMPORTANT)

### Branching
- `main` branch must always be in a known-working state
- All work must be done in feature branches
- Each milestone should be completed via a Pull Request

### Scope Control
- Work on **one milestone at a time**
- Do not implement future milestones early
- Do not refactor unrelated code

### Timing & Interrupts
- Lap detection must use **hardware interrupts**
- Timestamp must be captured immediately in the ISR
- No blocking code inside interrupts

### Stability
- No dynamic memory allocation in time-critical paths
- UI updates must not block timing or logging
- SD card writes must be buffered and non-blocking

---

## 7. Development Workflow

1. Select the next milestone from `docs/MILESTONES.md`
2. Create a feature branch (e.g. `feature/milestone-1-display`)
3. Implement only the scoped functionality
4. Run all applicable tests from `docs/TEST_PLAN.md`
5. Open a Pull Request with:
   - Summary of changes
   - Tests performed
   - Any known limitations
6. Merge only after milestone Definition of Done is met

---

## 8. Initial Milestone

Development begins with:

**Milestone 1 – Display Bring-Up**

Goal:
- Initialize display
- Render static text
- Render an uptime counter

No touch input, no IR, no SD card for this milestone.

---

## 9. Flashing & Debugging

- Flash firmware using Arduino IDE or CLI
- Monitor serial output at 115200 baud
- Use serial logs for early-stage verification before UI is active

---

## 10. Out of Scope (Version 1.0)

The following features are explicitly excluded:
- GPS lap detection
- Wireless communication (Bluetooth / Wi-Fi)
- Multi-kart support
- Cloud analytics
- Wheel speed sensors

---

## 11. Contribution Guidance for Codex / AI Assistants

When working on this repository:

- Read all documents in `docs/` before coding
- Follow the milestone order strictly
- Ask for clarification via issues if requirements are unclear
- Prefer clarity and determinism over cleverness

---

## 12. License

License to be defined.

---

## 13. Status

Project Phase: **Active Development**  
Current Focus: **Milestone 1 – Display Bring-Up**

### Driver integrity rule
Files under `firmware/pilaptimer/` that originate from Waveshare (display/touch drivers and fonts)
must be treated as read-only unless a change is explicitly requested. Modifying them can break
display initialization.
