# Key Engineering Decisions

## 2026-01-24 – IR Frame Detection Strategy
- Decision: Use framed burst pattern instead of continuous bursts
- Reason: TSOP AGC behavior + sunlight robustness
- Outcome: Stable outdoor detection at ~15 ft

## 2026-01-24 – Receiver Architecture (Phase 1)
- Decision: RP2350 handles IR directly
- Reason: Simplify early testing, reduce system variables
- Deferred: ESP32 wireless receiver (Phase 2)
