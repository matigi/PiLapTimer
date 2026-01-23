# Milestone 3 — IR Lap Trigger Input (Receiver)

## Goal
Detect a lap trigger via an external IR receiver connected to a GPIO pin and generate a reliable "lap event" timestamp without false triggers.

## Non-goals (out of scope)
- SD logging
- Full session state machine
- IMU recording
- Touch UI menus
- Multi-car IDs / decoding protocols beyond pulse detection

## Inputs / Signals
- IR receiver digital output connected to a GPIO input (3.3V logic).
- Signal is expected to pulse when an IR beacon is seen.

## Functional Requirements
1. **Interrupt-based detection**
   - Use GPIO interrupt (rising or falling edge as appropriate).
   - ISR must be minimal: capture timestamp + set a flag/counter only.

2. **Debounce / lockout window**
   - After a lap event, ignore additional triggers for `LAP_LOCKOUT_MS` (default 1500 ms).
   - Lockout must be enforced outside ISR (but ISR can early-return if within lockout).

3. **Timestamp accuracy**
   - Timestamp resolution target: 1 ms or better.
   - Use `millis()` for v1; optionally `micros()` if stable on this core.

4. **UI feedback**
   - On-screen: show last lap timestamp (relative time since boot is fine for this milestone).
   - On-screen: show lap count.
   - Audible feedback optional (buzzer later milestone).

5. **Serial debug (minimal)**
   - Print one line per lap event:
     - lap number
     - timestamp
     - time since previous lap (if available)

## Configuration
- `IR_PIN` (GPIO number)
- `IR_EDGE` (RISING/FALLING/CHANGE)
- `LAP_LOCKOUT_MS` default 1500
- `USE_MICROS` boolean (default false)

## Acceptance Tests
- **Test 3.1 — Single pass**
  - Trigger IR once: lap count increments by 1, timestamp captured.
- **Test 3.2 — Rapid repeats**
  - Trigger multiple times within lockout: only 1 lap recorded.
- **Test 3.3 — Two valid laps**
  - Trigger twice separated by > lockout: lap count increments twice; delta shown.
- **Test 3.4 — ISR health**
  - No display corruption or freezing while triggering repeatedly.

## Notes / Implementation Guidance
- ISR should only:
  - read current time (millis/micros)
  - store into volatile variables
  - set a volatile flag/counter
- Main loop reads the flag and performs:
  - lockout enforcement
  - UI update
  - serial print
