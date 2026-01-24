# Project Status – PiLapTimer

## Current Phase
Phase 1 – Wired IR lap timing using RP2350 (no wireless)

## Locked Decisions
- IR beacon:
  - ESP32-S3, GPIO9
  - 38 kHz carrier
  - Burst-gated only (no continuous IR)
  - Pattern: 10 × (2ms ON / 2ms OFF) + ~20ms gap
- IR receiver:
  - Demodulating 38 kHz (TSOP / VS1838 class)
  - Connected directly to RP2350 GPIO2
  - Frame-based detection + cooldown
- Cooldown-based lap triggering (2s default)

## Known-Good Parameters
- FRAME_GAP_MIN_US ≈ 19 ms
- MIN_VALID_BURSTS_FOR_FRAME = 10
- BURST_WINDOW_MS = 120 ms

## Explicitly Out of Scope (for now)
- Wireless telemetry
- ESP32 on kart
- Continuous IR illumination
- Ultra-long range tuning

## Next Milestone
Integrate IR receiver core into RP2350 UI and lap timing state machine.

## Display Bring-Up (Locked)
- AMOLED init MUST follow Waveshare demo sequence
- QSPI_GPIO_Init → QSPI_PIO_Init → QSPI_1Wrie_Mode → AMOLED_1IN64_Init
- Full framebuffer required for AMOLED_1IN64_Display()
