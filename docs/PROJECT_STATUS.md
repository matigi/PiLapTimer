# PiLapTimer – Project Status

Last updated: SD logging validation + next IMU logging focus

# Project Status (Authoritative)

This file is the authoritative source for milestone completion and current development focus.

The README provides a high-level summary only.

---

## What Works Today

- RP2350 AMOLED display initializes and updates correctly
- Touch controller (FT3168) reads coordinates reliably
- Time-attack UI flow: driver selection, lap count selection, arming, run, results
- Rotated landscape UI (456x280 logical) with larger running lap/time readouts
- IR lap detection starts runs, records laps, and finishes at target laps
- Buzzer beeps on lap events and completion
- Display stays stable with full UI flow
- SD sessions created under `/PILAPTIMER/SESSIONS` with lap + reaction logs
- G-force monitor tile responds smoothly with corrected axis orientation

Verified via:
- Serial logs
- Oscilloscope captures
- On-device time-attack session testing (touch menus, lap timing, beeper)

---

## Known-Good Commit / Tag

- **Tag:** `hw-m05-touch-beeper-ok`
- This tag represents a stable baseline for touch + beeper bring-up
- Future work must build on this tag

---

## Hardware Summary

- RP2350 Touch AMOLED 1.64
- IR receiver powered at 3.3V
- IR OUT → GPIO1 (direct)
- ESP32-S3 IR beacon (burst-gated)

---

## Current Focus

Current focus is **IMU logging milestone (Milestone 9)**.

Specifically:
- Maintain UI stability and readability
- Capture IMU samples at 100–200 Hz and log to SD
- Capture any remaining UX tweaks before moving to wireless milestones

---

## Known Risks / Notes

- Touch controller can glitch (0xFF); hardened reads mitigate stuck-down states
- Buzzer pin must avoid GP6/GP7 (I2C)

---

## Not Started Yet

- IMU data logging to SD (Milestone 9)
- Wireless ESP32 integration
- Multi-kart support
