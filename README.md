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

# UI Splash Assets

Static UI assets used by firmware.

Rules:
- Files in this directory are treated as read-only assets
- Do not modify without updating the source image
- Firmware should reference these assets but not generate or alter them
