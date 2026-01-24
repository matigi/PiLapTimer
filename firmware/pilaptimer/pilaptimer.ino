#include <Arduino.h>

#include "DEV_Config.h"
#include "AMOLED_1in64.h"
#include "FT3168.h"
#include "GUI_Paint.h"
#include "fonts.h"
#include "qspi_pio.h"

// Make IRAM_ATTR portable (ESP32 uses it; RP/Pico cores don't)
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// RGB565 colors
#ifndef RED
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F
#define WHITE 0xFFFF
#define BLACK 0x0000
#endif

namespace {
constexpr uint16_t kInfoHeight = 80;
static UWORD info_image[AMOLED_1IN64_WIDTH * kInfoHeight];
static bool last_touch_active = false;
static uint16_t last_touch_x = 0;
static uint16_t last_touch_y = 0;

constexpr uint8_t kIrPin = 2;
constexpr bool kIrActiveLow = true;
constexpr uint32_t kLapDebounceMs = 1500;
constexpr uint32_t kMinLapTimeMs = 5000;

volatile bool ir_triggered = false;
volatile uint32_t ir_timestamp_ms = 0;

uint32_t lap_count = 0;
uint32_t last_valid_lap_ms = 0;
uint32_t last_trigger_ms = 0;
}  // namespace

void IRAM_ATTR OnIrTrigger() {
  ir_timestamp_ms = millis();
  ir_triggered = true;
}

void RenderTouchInfo(bool touch_active, uint16_t x, uint16_t y) {
  Paint_SelectImage(reinterpret_cast<UBYTE *>(info_image));
  Paint_Clear(BLACK);
  Paint_DrawString_EN(8, 6, "Touch Input", &Font20, WHITE, BLACK);

  if (touch_active) {
    char x_text[16];
    char y_text[16];
    snprintf(x_text, sizeof(x_text), "X: %03u", x);
    snprintf(y_text, sizeof(y_text), "Y: %03u", y);
    Paint_DrawString_EN(8, 32, x_text, &Font20, GREEN, BLACK);
    Paint_DrawString_EN(8, 54, y_text, &Font20, GREEN, BLACK);
  } else {
    Paint_DrawString_EN(8, 38, "No touch", &Font16, RED, BLACK);
  }

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

  pinMode(kIrPin, kIrActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(kIrPin), OnIrTrigger,
                  kIrActiveLow ? FALLING : RISING);
  Serial.print("IR debounce ms=");
  Serial.println(kLapDebounceMs);
  Serial.print("Min lap ms=");
  Serial.println(kMinLapTimeMs);

  Serial.println("DONE");
}

void loop() {
  if (ir_triggered) {
    uint32_t timestamp_ms = 0;
    noInterrupts();
    if (ir_triggered) {
      timestamp_ms = ir_timestamp_ms;
      ir_triggered = false;
    }
    interrupts();

    if (timestamp_ms != 0) {
      if (last_trigger_ms != 0) {
        uint32_t trigger_delta_ms = timestamp_ms - last_trigger_ms;
        if (trigger_delta_ms < kLapDebounceMs) {
          Serial.printf("IR IGNORE (debounce) @ %lu ms | delta=%lu ms\n",
                        static_cast<unsigned long>(timestamp_ms),
                        static_cast<unsigned long>(trigger_delta_ms));
          return;
        }
      }

      last_trigger_ms = timestamp_ms;

      if (last_valid_lap_ms != 0) {
        uint32_t lap_delta_ms = timestamp_ms - last_valid_lap_ms;
        if (lap_delta_ms < kMinLapTimeMs) {
          Serial.printf("IR IGNORE (min lap) @ %lu ms | delta=%lu ms\n",
                        static_cast<unsigned long>(timestamp_ms),
                        static_cast<unsigned long>(lap_delta_ms));
        } else {
          lap_count++;
          last_valid_lap_ms = timestamp_ms;
          Serial.printf("IR HIT @ %lu ms | lap=%lu | delta=%lu ms\n",
                        static_cast<unsigned long>(timestamp_ms),
                        static_cast<unsigned long>(lap_count),
                        static_cast<unsigned long>(lap_delta_ms));
        }
      } else {
        lap_count++;
        last_valid_lap_ms = timestamp_ms;
        Serial.printf("IR HIT @ %lu ms | lap=%lu | delta=0 ms\n",
                      static_cast<unsigned long>(timestamp_ms),
                      static_cast<unsigned long>(lap_count));
      }
    }
  }

  bool touch_active = FT3168_Get_Point();
  uint16_t touch_x = FT3168.x_point;
  uint16_t touch_y = FT3168.y_point;

  if (touch_active != last_touch_active ||
      (touch_active &&
       (touch_x != last_touch_x || touch_y != last_touch_y))) {
    RenderTouchInfo(touch_active, touch_x, touch_y);
    last_touch_active = touch_active;
    last_touch_x = touch_x;
    last_touch_y = touch_y;
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
