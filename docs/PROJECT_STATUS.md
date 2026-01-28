# PiLapTimer – Project Status

Last updated: hw-m05-touch-beeper-ok baseline

# Project Status (Authoritative)

This file is the authoritative source for milestone completion and current development focus.

The README provides a high-level summary only.

---

## What Works Today

- RP2350 AMOLED display initializes and updates correctly
- Touch controller (FT3168) reads coordinates reliably
- Touch UI test screen renders raw + normalized coordinates
- Buzzer beeps on touch events
- Display stays stable with simplified firmware

Verified via:
- Serial logs
- Oscilloscope captures
- Manual IR blocking/unblocking tests

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

Current focus is **hw-m05: touch + beeper recovery baseline**.

Specifically:
- Confirm stable touch down/up detection
- Verify buzzer output on touch
- Keep the demo UI + touch crosshair stable
- Reintroduce lap-timer features once input/output are verified

---

## Known Risks / Notes

- Touch controller can glitch (0xFF); hardened reads mitigate stuck-down states
- Buzzer pin must avoid GP6/GP7 (I2C)

---

## Not Started Yet

- Reintroducing IR lap detection
- Driver selection + competitive runs
- Wireless ESP32 integration
- Data storage
- Multi-kart support
