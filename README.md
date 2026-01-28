## Project Status

**Current Phase:** Recovery – Touch + Beeper Baseline  
**Current Milestone:** hw-m05-touch-beeper-ok

Milestones 1–4 (Display bring-up, IR detection, basic lap timing) were complete,
but the current focus is restoring a reliable touch + beeper baseline before
reintroducing lap timing and session UX.

For detailed milestone tracking, see:
docs/PROJECT_STATUS.md

- docs/UI_FLOW_PHASE2.md – Phase 2 touchscreen UI flow and transitions

- ## Touch + Buzzer Stable Baseline

Tag: `pilaptimer-touch-stable-v1`

- FT3168 touch reading uses raw register block (0x02–0x06)
- Robust DOWN/UP detection via time-windowing
- Buzzer uses GPIO/PWM-safe method (no Arduino `tone()`)
- Multi-touch works
- Touch coordinates accurate

This tag represents the last known-good hardware integration baseline.

## Specification Hierarchy

This project has multiple functional specifications by phase.

**Authoritative (Current):**
- docs/FUNCTIONAL_SPEC_v2.md  
  (Phase 2 – Competitive Time Attack, multi-driver UX)

**Historical / Reference:**
- docs/gokart-lap-timer-functional-spec.md  
  (Phase 1 – Core lap timing, IR detection)

If there is any conflict, FUNCTIONAL_SPEC_v2.md takes precedence.

For full documentation index and authority rules, see:
docs/README.md

## LVGL Time Attack HUD UI (RP2350 Arduino)

This repo now includes an LVGL v8.4 UI port and a modern Time Attack HUD in
`firmware/pilaptimer`. The port uses a partial RGB565 draw buffer sized to
456×40 lines to keep RAM usage low, while preserving the existing 456×280
logical landscape orientation.

### Install LVGL

Option A (Arduino Library Manager):
1. Open **Tools → Manage Libraries…**
2. Search for **lvgl** and install **LVGL v8.4.x**

Option B (manual):
1. Download LVGL v8.4.x from https://github.com/lvgl/lvgl
2. Place it in your Arduino libraries folder as `lvgl/`

The sketch expects `lv_conf.h` to live alongside the `.ino` file (this repo
provides one in `firmware/pilaptimer/lv_conf.h`).

### Enable or Disable LVGL UI

The build uses a compile-time flag:
- `USE_LVGL_UI=1` → LVGL HUD (default)
- `USE_LVGL_UI=0` → legacy UI

To override, add a build flag (e.g. in Arduino IDE “Build Options”):
```
-DUSE_LVGL_UI=0
```

### Touch + UI Test Checklist

1. Flash `firmware/pilaptimer/pilaptimer.ino`.
2. Confirm the HUD shows a large lap timer, delta pill, best time, lap count,
   and the Start/Stop + Reset buttons.
3. Tap **Start** → the unit should arm and wait for IR start.
4. Tap **Reset** → return to idle (running state disables Reset).
5. Verify touch coordinates track buttons accurately in the rotated 456×280
   landscape layout.

# UI Splash Assets

Static UI assets used by firmware.

Rules:
- Files in this directory are treated as read-only assets
- Do not modify without updating the source image
- Firmware should reference these assets but not generate or alter them
