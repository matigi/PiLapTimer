#include <Arduino.h>

// Make IRAM_ATTR portable (ESP32 uses it; RP cores don't)
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// ================== USER CONFIG ==================
#define IR_IN_PIN 2

// Your beacon pattern: 10x(2ms ON / 2ms OFF) then ~20ms gap
// Tune: be stricter so we don't "see" phantom gaps as frames.
static const uint32_t FRAME_GAP_MIN_US = 19000;  // OFF >= 19ms counts as frame gap (20ms nominal)

// Burst timing tolerances (demod output, not the 38k carrier)
static const uint32_t ON_MIN_US  = 900;
static const uint32_t ON_MAX_US  = 4000;
static const uint32_t OFF_MIN_US = 900;
static const uint32_t OFF_MAX_US = 5000;         // tightened from 6000

// Require full burst train before a frame gap counts
static const uint8_t MIN_VALID_BURSTS_FOR_FRAME = 10;

// Require the burst train to complete within this window (prevents “slow accumulation” from noise)
static const uint32_t BURST_WINDOW_MS = 120;

// Lap trigger cooldown (loop-enforced)
static const uint32_t COOLDOWN_MS = 2000;
// ================================================

static inline bool inRange(uint32_t v, uint32_t lo, uint32_t hi) {
  return (v >= lo && v <= hi);
}

// ISR-shared state
volatile uint32_t lastEdgeUs = 0;
volatile bool lastLevel = true;          // HIGH idle (typical)
volatile uint32_t lastOnUs = 0;          // store plausible ON duration for pairing

volatile uint16_t validBurstCount = 0;
volatile uint16_t framesSeen = 0;
volatile uint16_t noiseEdges = 0;

volatile uint32_t burstWindowStartMs = 0;

// ISR publishes “frame candidate occurred”
volatile uint32_t pendingFrameMs = 0;

void IRAM_ATTR onIrEdge() {
  uint32_t nowUs = (uint32_t)micros();
  bool level = digitalRead(IR_IN_PIN); // true=HIGH, false=LOW

  if (lastEdgeUs == 0) {
    lastEdgeUs = nowUs;
    lastLevel = level;
    return;
  }

  uint32_t durUs = nowUs - lastEdgeUs;

  // lastLevel segment lasted durUs
  if (lastLevel == false) {
    // Measured ON (LOW) duration
    if (inRange(durUs, ON_MIN_US, ON_MAX_US)) {
      lastOnUs = durUs;
    } else {
      lastOnUs = 0;
      noiseEdges++;
    }
  } else {
    // Measured OFF (HIGH) duration
    uint32_t nowMs = (uint32_t)millis();

    // Enforce burst window timeout (prevents slow noise accumulation)
    if (burstWindowStartMs != 0 && (nowMs - burstWindowStartMs) > BURST_WINDOW_MS) {
      validBurstCount = 0;
      burstWindowStartMs = 0;
      lastOnUs = 0;
    }

    if (durUs >= FRAME_GAP_MIN_US) {
      // Long OFF gap: possible frame boundary
      if (validBurstCount >= MIN_VALID_BURSTS_FOR_FRAME) {
        framesSeen++;
        pendingFrameMs = nowMs; // publish to loop
      }

      // Reset at any long gap
      validBurstCount = 0;
      burstWindowStartMs = 0;
      lastOnUs = 0;

    } else if (inRange(durUs, OFF_MIN_US, OFF_MAX_US)) {
      // Normal OFF between bursts: count only if we had a valid ON just before
      if (lastOnUs != 0) {
        if (validBurstCount == 0) {
          burstWindowStartMs = nowMs; // start window on first valid burst
        }
        validBurstCount++;
        lastOnUs = 0; // consume
      } else {
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

  pinMode(IR_IN_PIN, INPUT_PULLUP);

  lastEdgeUs = 0;
  lastLevel = digitalRead(IR_IN_PIN);

  attachInterrupt(digitalPinToInterrupt(IR_IN_PIN), onIrEdge, CHANGE);

  Serial.println();
  Serial.println("RP2350 IR Receiver Bench Test (tuned framed beacon)");
  Serial.printf("GPIO=%d | FRAME_GAP_MIN_US=%lu | MIN_BURSTS=%u | WINDOW_MS=%lu | COOLDOWN_MS=%lu\n",
                IR_IN_PIN,
                (unsigned long)FRAME_GAP_MIN_US,
                (unsigned)MIN_VALID_BURSTS_FOR_FRAME,
                (unsigned long)BURST_WINDOW_MS,
                (unsigned long)COOLDOWN_MS);
  Serial.printf("ON range: %lu..%lu us | OFF range: %lu..%lu us\n",
                (unsigned long)ON_MIN_US, (unsigned long)ON_MAX_US,
                (unsigned long)OFF_MIN_US, (unsigned long)OFF_MAX_US);
}

void loop() {
  static uint32_t lastPrintMs = 0;
  static uint32_t lastLapMs = 0;

  uint32_t now = millis();

  // Consume pending frame candidate (atomic copy+clear)
  uint32_t frameMs = 0;
  noInterrupts();
  frameMs = pendingFrameMs;
  pendingFrameMs = 0;
  interrupts();

  if (frameMs != 0) {
    // Cooldown enforced here (deterministic)
    if ((frameMs - lastLapMs) >= COOLDOWN_MS) {
      lastLapMs = frameMs;

      uint16_t f = 0, n = 0;
      noInterrupts();
      f = framesSeen;
      n = noiseEdges;
      interrupts();

      Serial.printf("LAP TRIGGERED @ %lu ms | framesSeen=%u | noiseEdges=%u\n",
                    (unsigned long)frameMs, (unsigned)f, (unsigned)n);
    }
  }

  // Periodic stats
  if (now - lastPrintMs >= 500) {
    lastPrintMs = now;

    uint16_t bursts = 0, f = 0, n = 0;
    bool lvl = true;
    uint32_t winStart = 0;

    noInterrupts();
    bursts = validBurstCount;
    f = framesSeen;
    n = noiseEdges;
    lvl = lastLevel;
    winStart = burstWindowStartMs;
    interrupts();

    Serial.printf("t=%lu ms | level=%s | bursts=%u | frames=%u | noise=%u | winAge=%lu ms\n",
                  (unsigned long)now,
                  lvl ? "HIGH" : "LOW",
                  (unsigned)bursts,
                  (unsigned)f,
                  (unsigned)n,
                  (winStart == 0) ? 0UL : (unsigned long)(now - winStart));
  }
}
