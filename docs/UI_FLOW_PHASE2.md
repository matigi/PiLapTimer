# PiLapTimer – Touch UI Flow (Phase 2)

## Competitive Time Attack Mode

---

## 1. Purpose

This document defines the exact touchscreen UI screens, layouts, touch targets, and state transitions for Phase 2 of the PiLapTimer project.

This document is authoritative for UI behavior and must not contradict:
- FUNCTIONAL_SPEC_v2.md
- CODEX_HANDOFF.md

If conflicts arise, this document governs screen flow and interaction behavior.

---

## 2. UI Design Principles

- Touch UI must be simple and readable at a glance
- No configuration changes allowed during an active run
- All state transitions must be explicit and predictable
- Touch input must never interfere with IR detection
- Accidental touches during driving must not affect state

---

## 3. Screen Index

| Screen ID | Name |
|---------|------|
| S0 | Boot / Splash |
| S1 | Idle / Ready |
| S2 | Armed (Waiting for Start) |
| S3 | Running |
| S4 | Finished / Results |
| S5 | Driver Stats / Comparison |

---

## 4. Screen Definitions

---

### S0 — Boot / Splash

**Purpose:**  
Confirm device is powered and initializing.

**Display:**
Displays static splash image from assets/ui/splash/ before entering Idle state.

**Behavior:**
- Shown briefly on boot (0.5–1.0 s)
- Automatically transitions to S1

**Touch:**
- Ignored

---

### S1 — Idle / Ready (Home Screen)

**Purpose:**  
Select driver and lap count before starting a run.

**Display:**
Driver: 3 [ − ] [ + ]
Laps: 5 [ − ] [ + ]

Last Run:
Total: 1:23.456
Best: 0:15.892

[ START RUN ]

**Touch Targets:**

| Button | Action |
|------|--------|
| Driver − | Decrement driver ID (wrap 1↔10) |
| Driver + | Increment driver ID |
| Laps − | Decrement lap count (min = 1) |
| Laps + | Increment lap count |
| START RUN | Transition to S2 (Armed) |

**Rules:**
- Driver and lap count changes take effect immediately
- “Last Run” reflects the selected driver
- START RUN is disabled if lap count is invalid

---

### S2 — Armed (Pre-Run)

**Purpose:**  
Indicate readiness and wait for the first beacon crossing.

**Display:**
Driver: 3
Target Laps: 5

READY
Cross start line to begin

**Behavior:**
- No timing active
- No laps recorded
- First IR beacon detection:
  - Starts session timer
  - Transitions to S3 (Running)

**Touch Targets:**

| Button | Action |
|------|--------|
| CANCEL | Abort and return to S1 |

---

### S3 — Running (Active Session)

**Purpose:**  
Show run progress clearly while driving.

**Display:**
Driver: 3
Lap: 2 / 5

Last Lap: 0:16.201
Best Lap: 0:15.892

Session: 0:34.802

(Optional indicator)
● IR OK

**Behavior:**
- Each beacon detection:
  - Increment lap count
  - Record lap time
  - Emit short beep
- When `lap_count == target_laps`:
  - Finalize session
  - Emit completion beep
  - Transition to S4

**Touch:**
- All touch input ignored during this screen

---

### S4 — Finished / Results

**Purpose:**  
Display final results and allow review.

**Display:**
Driver: 3
Laps: 5

Total: 1:23.456
Best: 0:15.892
Avg: 0:16.691

[ VIEW STATS ] [ DONE ]

**Touch Targets:**

| Button | Action |
|------|--------|
| VIEW STATS | Transition to S5 |
| DONE | Return to S1 |

**Rules:**
- Results are frozen
- No automatic transitions

---

### S5 — Driver Stats / Comparison

**Purpose:**  
Encourage competition by showing recent driver performance.

**Display:**
Driver 3 – Last Run
Total: 1:23.456
Best: 0:15.892
Avg: 0:16.691

Previous Driver:
Driver 2
Best: 0:16.104

[ BACK ]

**Behavior:**
- Displays:
  - Selected driver’s most recent run
  - Most recent run by another driver (or best overall)

**Touch Targets:**

| Button | Action |
|------|--------|
| BACK | Return to S4 |

---

## 5. State Transition Map (Authoritative)

BOOT
↓
S1 – IDLE
├── START RUN → S2 – ARMED
│ ├── Beacon → S3 – RUNNING
│ └── CANCEL → S1
│
S3 – RUNNING
└── lap == target → S4 – FINISHED
├── VIEW STATS → S5
│ └── BACK → S4
└── DONE → S1

---

## 6. UX Rules (Mandatory)

1. Configuration changes are forbidden during RUNNING
2. First beacon crossing always starts timing
3. Touch input is ignored during RUNNING
4. Audible feedback must occur on every lap
5. Completion beep must be distinct
6. IDLE screen must always be a safe recovery state

---

## 7. Implementation Notes

- Each screen should have a dedicated render function
- Screen redraws occur only on:
  - State change
  - Data update (lap, time, touch)
- UI rendering must not block the main loop
- IR detection and timing logic must operate independently
- Touch hitboxes should be defined explicitly and reused

---

## 8. Scope Control

This UI flow applies only to Phase 2.

Out of scope:
- Wireless features
- Persistent storage
- Multi-kart coordination
- Networked leaderboards

These may be addressed in later phases.
