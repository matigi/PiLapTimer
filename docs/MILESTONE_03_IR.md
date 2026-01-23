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

## Receiver Bench Test Sketch (Milestone 3 Progress)
The following sketch is the current bench-test receiver code used to validate IR receiver performance against the framed beacon (2 ms on / 2 ms off bursts with a ~20 ms gap). It counts valid bursts, detects frame gaps, and triggers a lap event with a cooldown to prevent double counts.

```cpp
#include <Arduino.h>

// Make IRAM_ATTR portable (ESP32 uses it; RP/Pico cores don't)
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ================== USER CONFIG ==================
#define IR_IN_PIN 2               // RP2350 GPIO2
#define IR_ACTIVE_LOW true        // TSOP/VS1838 output: LOW when IR burst present

// Beacon pattern (from your framed ESP32 sketch)
static const uint32_t EXPECT_ON_US  = 2000;   // 2ms (informational)
static const uint32_t EXPECT_OFF_US = 2000;   // 2ms (informational)

// Frame gap: your beacon uses 20ms nominal; tighten detection
static const uint32_t FRAME_GAP_MIN_US = 16000; // OFF >= 16ms counts as frame gap

// Tolerances (conservative; we only need "roughly 2ms")
static const uint32_t ON_MIN_US  = 900;
static const uint32_t ON_MAX_US  = 4000;
static const uint32_t OFF_MIN_US = 900;
static const uint32_t OFF_MAX_US = 6000;

// How many valid bursts needed before a gap counts as a frame
static const uint8_t MIN_VALID_BURSTS_FOR_FRAME = 8;

// Lap trigger cooldown
static const uint32_t COOLDOWN_MS = 2000;
// ================================================

// ISR-shared state
volatile uint32_t lastEdgeUs = 0;
volatile bool lastLevel = true; // HIGH idle
volatile uint16_t validBurstCount = 0;
volatile uint16_t framesSeen = 0;
volatile uint16_t noiseEdges = 0;

// Store last ON duration so we only count a burst on a paired ON+OFF cycle
volatile uint32_t lastOnUs = 0;

volatile uint32_t lastFrameMs = 0;     // cooldown gating
volatile bool lapTriggeredFlag = false;

// Helper: classify durations
static inline bool inRange(uint32_t v, uint32_t lo, uint32_t hi) {
  return (v >= lo && v <= hi);
}

void IRAM_ATTR onIrEdge() {
  uint32_t nowUs = (uint32_t)micros();
  bool level = digitalRead(IR_IN_PIN); // true=HIGH, false=LOW

  // First edge initialization
  if (lastEdgeUs == 0) {
    lastEdgeUs = nowUs;
    lastLevel = level;
    return;
  }

  uint32_t durUs = nowUs - lastEdgeUs;

  // We just finished a segment at "lastLevel" that lasted durUs
  // lastLevel == LOW  => measured ON time (IR burst present)
  // lastLevel == HIGH => measured OFF time (idle)
  if (lastLevel == false) {
    // ON duration
    if (inRange(durUs, ON_MIN_US, ON_MAX_US)) {
      lastOnUs = durUs; // remember we saw a plausible ON chunk
    } else {
      lastOnUs = 0;
      noiseEdges++;
    }
  } else {
    // OFF duration
    if (durUs >= FRAME_GAP_MIN_US) {
      // Long OFF gap: frame boundary
      if (validBurstCount >= MIN_VALID_BURSTS_FOR_FRAME) {
        framesSeen++;

        // Cooldown gating for lap trigger
        uint32_t nowMs = (uint32_t)millis();
        if ((nowMs - lastFrameMs) >= COOLDOWN_MS) {
          lastFrameMs = nowMs;
          lapTriggeredFlag = true;
        }
      }

      // Reset counters at any long gap
      validBurstCount = 0;
      lastOnUs = 0;
    } else if (inRange(durUs, OFF_MIN_US, OFF_MAX_US)) {
      // Normal OFF between bursts: only count if we previously saw a valid ON
      if (lastOnUs != 0) {
        validBurstCount++;
        lastOnUs = 0; // consume so we require a paired cycle next time
      } else {
        // OFF looked valid but didn't follow a valid ON (likely noise)
        noiseEdges++;
      }
    } else {
      noiseEdges++;
      lastOnUs = 0;
    }
  }

  lastEdgeUs = nowUs;
  lastLevel = level;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(IR_IN_PIN, INPUT_PULLUP); // most TSOP/VS1838 drive actively; pullup helps if floating

  // Initialize levels/timestamps
  lastEdgeUs = 0;
  lastLevel = digitalRead(IR_IN_PIN);

  attachInterrupt(digitalPinToInterrupt(IR_IN_PIN), onIrEdge, CHANGE);

  Serial.println();
  Serial.println("RP2350 IR Receiver Bench Test (framed beacon)");
  Serial.println("GPIO2 input, expecting ~10x (2ms on/2ms off) then ~20ms gap.");
  Serial.printf("FRAME_GAP_MIN_US=%lu, MIN_VALID_BURSTS_FOR_FRAME=%u, COOLDOWN_MS=%lu\n",
                (unsigned long)FRAME_GAP_MIN_US,
                (unsigned)MIN_VALID_BURSTS_FOR_FRAME,
                (unsigned long)COOLDOWN_MS);
  Serial.printf("ON range: %lu..%lu us | OFF range: %lu..%lu us\n",
                (unsigned long)ON_MIN_US, (unsigned long)ON_MAX_US,
                (unsigned long)OFF_MIN_US, (unsigned long)OFF_MAX_US);
}

void loop() {
  static uint32_t lastPrintMs = 0;
  uint32_t now = millis();

  // If we triggered a "lap" event
  if (lapTriggeredFlag) {
    noInterrupts();
    lapTriggeredFlag = false;
    uint16_t f = framesSeen;
    uint16_t n = noiseEdges;
    interrupts();

    Serial.printf("LAP TRIGGERED @ %lu ms | framesSeen=%u | noiseEdges=%u\n",
                  (unsigned long)now, (unsigned)f, (unsigned)n);
  }

  // Periodic stats
  if (now - lastPrintMs >= 500) {
    lastPrintMs = now;

    noInterrupts();
    uint16_t bursts = validBurstCount;
    uint16_t f = framesSeen;
    uint16_t n = noiseEdges;
    bool lvl = lastLevel;
    interrupts();

    Serial.printf("t=%lu ms | level=%s | bursts=%u | frames=%u | noise=%u\n",
                  (unsigned long)now,
                  lvl ? "HIGH" : "LOW",
                  (unsigned)bursts,
                  (unsigned)f,
                  (unsigned)n);
  }
}
```
