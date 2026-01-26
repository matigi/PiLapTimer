# PiLapTimer – Project Status

Last updated: Phase 1, Milestone 3 complete

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

Next work is **user-facing UX and session logic**, not signal detection.

Specifically:
- Session start/stop/reset
- Lap timing display
- Best/last lap tracking
- CSV logging over Serial

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
