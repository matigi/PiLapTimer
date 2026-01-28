# PiLapTimer – Project Status

Last updated: time-attack UI rotation + readability polish

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

Current focus is **post-hw-m06 polish + next milestone planning**.

Specifically:
- Maintain UI stability and readability
- Capture any remaining UX tweaks before moving to storage/wireless milestones

---

## Known Risks / Notes

- Touch controller can glitch (0xFF); hardened reads mitigate stuck-down states
- Buzzer pin must avoid GP6/GP7 (I2C)

---

## Not Started Yet

- Wireless ESP32 integration
- Data storage
- Multi-kart support
