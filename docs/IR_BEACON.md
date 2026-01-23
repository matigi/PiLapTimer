# IR Beacon Firmware

This sketch drives a fixed start/finish IR beacon that transmits a 38 kHz carrier in short bursts.
The kart-mounted IR receiver should see these bursts as a clean digital pulse for lap detection.

## Behavior
- 38 kHz PWM carrier on `IR_PIN`.
- Bursts of `BURST_ON_MS` followed by `BURST_OFF_MS`.
- A packet is `BURSTS_PER_PACKET` bursts, followed by `FRAME_GAP_MS` of silence.
- Onboard RGB LED indicates status:
  - Blue: booting
  - Green: transmitting

## Hardware Wiring
- Connect an IR LED (with proper current limiting and transistor driver as needed) to `IR_PIN`.
- Ensure the IR LED driver can handle the 38 kHz PWM.

## Configuration
Edit these constants in `firmware/ir_beacon/ir_beacon.ino`:
- `IR_PIN`: GPIO for the IR LED driver input.
- `IR_FREQ`: Carrier frequency (default 38 kHz).
- `IR_RES`: PWM resolution.
- `IR_DUTY`: PWM duty value (default ~33% at 8-bit resolution).
- `BURST_ON_MS`, `BURST_OFF_MS`: Burst on/off timing.
- `BURSTS_PER_PACKET`: Number of bursts in a packet.
- `FRAME_GAP_MS`: Silence between packets.

## Build/Flash
Use the same Arduino-Pico toolchain as the kart firmware, but select the beacon board target
in the Arduino IDE/CLI. Then open and upload `firmware/ir_beacon/ir_beacon.ino`.
