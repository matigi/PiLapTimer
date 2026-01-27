# PiLapTimer – Project Status

Last updated: Phase 2, hw-m06 in progress

# Project Status (Authoritative)

This file is the authoritative source for milestone completion and current development focus.

The README provides a high-level summary only.

---

## What Works Today

- RP2350 AMOLED display initializes and updates correctly
- Touch controller (FT3168) reads coordinates reliably
- IR receiver detects beacon presence consistently
- Lap counter increments correctly
- Cooldown and minimum lap time enforced
- System stable without false triggers

Verified via:
- Serial logs
- Oscilloscope captures
- Manual IR blocking/unblocking tests

---

## Known-Good Commit / Tag

- **Tag:** `hw-m03-rx-touch-ir-ok`
- This tag represents a stable baseline
- Future work must build on this tag

---

## Hardware Summary

- RP2350 Touch AMOLED 1.64
- IR receiver powered at 3.3V
- IR OUT → GPIO1 (direct)
- ESP32-S3 IR beacon (burst-gated)

---

## Current Focus

Current focus is **hw-m06: driver selection + fixed-lap competitive runs**.

Specifically:
- Driver selection (1–10) on the idle screen
- Target lap selection before starting a run
- Armed → Running → Finished flow per UI_FLOW_PHASE2.md
- Results screen with total, best, and average lap times
- Non-blocking lap and completion beeps

---

## Known Risks / Notes

- Outdoor sunlight may reduce IR margin; hysteresis currently mitigates this
- IR thresholds may need minor tuning during outdoor field testing
- Beacon enclosure/aiming not finalized yet

---

## Not Started Yet

- Wireless ESP32 integration
- Data storage
- Multi-kart support
