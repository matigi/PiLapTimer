# PiLapTimer – Codex Handoff

This document is the single source of truth for Codex and future contributors.
Read this fully before making changes.

---

## Project Overview

**Goal:**  
Build a reliable outdoor go-kart lap timer using an IR beacon and an onboard receiver.

**Range target:** ~15 ft  
**Environment:** Outdoor daylight  
**Trigger method:** IR beacon presence detection with hysteresis + cooldown

---

## Project Phases

### Phase 1 (CURRENT)
- RP2350 touchscreen unit
- IR receiver wired directly to RP2350
- Reliable lap detection
- Touch UI
- Serial logging

### Phase 2 (FUTURE – NOT YET)
- ESP32 wireless link
- Telemetry / remote display
- Multi-kart support

**Do not implement Phase 2 features unless explicitly requested.**

---

## Known-Good Hardware Configuration (DO NOT CHANGE)

### IR Receiver
- Module type: 38 kHz demodulating IR receiver (TSOP / VS1838 class)
- **VCC:** 3.3V
- **GND:** Common ground
- **OUT:** Directly to RP2350 **GPIO1**
- **No voltage divider**
- GPIO configured as `INPUT_PULLUP`

⚠️ Powering the receiver at 5V or using a divider caused unreliable logic levels and must not be reintroduced.

### Optional (recommended)
- 0.1 µF ceramic capacitor across VCC–GND at the IR receiver

---

## Known-Good IR Detection Parameters (LOCK THESE)

These values were validated with scope + real beacon testing.

```text
Sample window:        20 ms
ON threshold:         ≥ 3 falling edges per window
OFF threshold:        ≤ 1 falling edge per window
Required streak:      2 consecutive windows
Cooldown:             2000 ms
Minimum lap time:     5000 ms
