# Known-Good Configuration (hw-m05)

This document captures the verified working baseline for the IR receiver and beacon.

## Receiver (RP2350)
- IR pin: GPIO1 / Arduino pin 1
- GPIO mode: `INPUT_PULLUP`
- ISR edge: `FALLING`
- Sampling window: 20 ms
- ON threshold: ≥ 3 falling edges per window
- OFF threshold: ≤ 1 falling edge per window
- Required streak: 2 consecutive windows
- Lap cooldown: 2000 ms
- Minimum lap time: 5000 ms

## Beacon (ESP32-S3)
- IR pin: GPIO9
- Carrier: 38 kHz (burst-gated)
- Burst gating: on/off envelope with a frame gap (no continuous illumination)

## Gotchas
- Touching exposed resistor leads on the receiver can create noise edges.
- Receiver modules that require 5V must level-shift to 3.3V on the OUT pin.
