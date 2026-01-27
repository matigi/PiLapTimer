// ==== known-good config ====
#include <Arduino.h>

#include "DEV_Config.h"
#include "AMOLED_1in64.h"
#include "qspi_pio.h"
#include "FT3168.h"
#include "GUI_Paint.h"
#include "fonts.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#ifndef WHITE
#define WHITE 0xFFFF
#define BLACK 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#endif

// ===== IR GPIO (your proven-good setup) =====
static const uint8_t IR_IN_PIN = 1;   // GP1 / Arduino pin 1
// ===========================================

// ===== Trigger tuning (known-good) =====
static const uint32_t SAMPLE_MS      = 20;  // cadence
static const uint16_t ON_THRESHOLD   = 3;   // falls per sample to say "present"
static const uint16_t OFF_THRESHOLD  = 1;   // falls per sample to say "absent"
static const uint8_t  STREAK_N       = 2;   // consecutive samples required

static const uint32_t LAP_COOLDOWN_MS = 2000;
static const uint32_t MIN_LAP_MS      = 5000;
// ======================================

static UWORD* gFrame = nullptr;

// Touch/UI state
static bool last_touch_active = false;
static uint16_t last_touch_x = 0;
static uint16_t last_touch_y = 0;

enum SessionState : uint8_t {
  STATE_IDLE = 0,
  STATE_RUNNING = 1,
};

static SessionState session_state = STATE_IDLE;

// Lap state
static uint32_t lap_count = 0;
static uint32_t last_lap_ms = 0;               // anchor for lap timing (set on session start + each accepted lap)
static uint32_t last_lap_duration_ms = 0;
static uint32_t best_lap_ms = 0;
static uint32_t session_start_ms = 0;

static const uint16_t BTN_WIDTH = 120;
static const uint16_t BTN_HEIGHT = 40;
static const uint16_t BTN_START_X = 8;
static const uint16_t BTN_STOP_X = 152;
static const uint16_t BTN_Y = 170;

// IR ISR counters
volatile uint32_t gFallingCount = 0;
volatile uint32_t gLastFallingUs = 0;

void IRAM_ATTR irFallingISR() {
  gFallingCount++;
  gLastFallingUs = (uint32_t)micros();
}

const char* SessionStateLabel(SessionState state) {
  return (state == STATE_RUNNING) ? "RUNNING" : "IDLE";
}

void DrawButton(uint16_t x, uint16_t y, const char* label, bool active) {
  UWORD border = active ? GREEN : WHITE;
  UWORD text = active ? GREEN : WHITE;
  Paint_DrawRectangle(x, y, x + BTN_WIDTH, y + BTN_HEIGHT, border, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawString_EN(x + 12, y + 12, label, &Font16, text, BLACK);
}

void DrawUI(bool beaconPresent, uint16_t fallsLast, uint8_t onStreak, uint8_t offStreak, uint32_t lastLowAgeMs) {
  if (!gFrame) return;

  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);

  char lap_text[32];
  snprintf(lap_text, sizeof(lap_text), "Lap: %lu", (unsigned long)lap_count);
  Paint_DrawString_EN(8, 32, lap_text, &Font20, GREEN, BLACK);

  char state_text[32];
  snprintf(state_text, sizeof(state_text), "State: %s", SessionStateLabel(session_state));
  Paint_DrawString_EN(8, 60, state_text, &Font16, WHITE, BLACK);

  char lap_time_text[40];
  snprintf(lap_time_text, sizeof(lap_time_text), "Last: %lu ms", (unsigned long)last_lap_duration_ms);
  Paint_DrawString_EN(8, 80, lap_time_text, &Font16, WHITE, BLACK);

  char best_text[40];
  snprintf(best_text, sizeof(best_text), "Best: %lu ms", (unsigned long)best_lap_ms);
  Paint_DrawString_EN(8, 100, best_text, &Font16, WHITE, BLACK);

  if (last_touch_active) {
    char t[48];
    snprintf(t, sizeof(t), "Touch %u,%u", last_touch_x, last_touch_y);
    Paint_DrawString_EN(8, 126, t, &Font16, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(8, 126, "No touch", &Font16, RED, BLACK);
  }

  char ir1[64];
  snprintf(ir1, sizeof(ir1), "IR pin=%u falls=%u/%lums", (unsigned)IR_IN_PIN, (unsigned)fallsLast, (unsigned long)SAMPLE_MS);
  Paint_DrawString_EN(8, 220, ir1, &Font16, WHITE, BLACK);

  char ir2[64];
  snprintf(ir2, sizeof(ir2), "Beacon=%s on=%u off=%u",
           beaconPresent ? "YES" : "NO",
           (unsigned)onStreak, (unsigned)offStreak);
  Paint_DrawString_EN(8, 240, ir2, &Font16, beaconPresent ? GREEN : RED, BLACK);

  char ir3[64];
  snprintf(ir3, sizeof(ir3), "lastLOW=%lums", (unsigned long)lastLowAgeMs);
  Paint_DrawString_EN(8, 260, ir3, &Font16, WHITE, BLACK);

  DrawButton(BTN_START_X, BTN_Y, "START", session_state == STATE_IDLE);
  DrawButton(BTN_STOP_X, BTN_Y, "STOP", session_state == STATE_RUNNING);

  AMOLED_1IN64_Display(gFrame);
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { delay(10); }

  Serial.println();
  Serial.println("BOOT: PiLapTimer receiver (stable trigger)");

  if (DEV_Module_Init() != 0) {
    Serial.println("DEV_Module_Init FAILED");
    while (true) delay(1000);
  }

  QSPI_GPIO_Init(qspi);
  QSPI_PIO_Init(qspi);
  QSPI_1Wrie_Mode(&qspi);

  AMOLED_1IN64_Init();
  AMOLED_1IN64_SetBrightness(100);
  AMOLED_1IN64_Clear(WHITE);

  UDOUBLE imageSizeBytes = (UDOUBLE)AMOLED_1IN64_HEIGHT * (UDOUBLE)AMOLED_1IN64_WIDTH * 2;
  gFrame = (UWORD*)malloc(imageSizeBytes);
  if (!gFrame) {
    Serial.println("framebuffer malloc FAILED");
    while (true) delay(1000);
  }

  Paint_NewImage((UBYTE*)gFrame, AMOLED_1IN64.WIDTH, AMOLED_1IN64.HEIGHT, 0, WHITE);
  Paint_SetScale(65);
  Paint_SetRotate(ROTATE_0);
  Paint_Clear(BLACK);
  AMOLED_1IN64_Display(gFrame);

  FT3168_Init(FT3168_Point_Mode);

  pinMode(IR_IN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_IN_PIN), irFallingISR, FALLING);

  Serial.printf("IR pin=%u, sample=%lums, on>=%u, off<=%u, streak=%u\n",
                (unsigned)IR_IN_PIN, (unsigned long)SAMPLE_MS,
                (unsigned)ON_THRESHOLD, (unsigned)OFF_THRESHOLD, (unsigned)STREAK_N);

  DrawUI(false, 0, 0, 0, 999999);
}

void loop() {
  uint32_t nowMs = millis();

  // Touch
  bool ta = FT3168_Get_Point();
  last_touch_active = ta;
  last_touch_x = FT3168.x_point;
  last_touch_y = FT3168.y_point;

  static bool prev_touch_active = false;
  static uint32_t last_touch_ms = 0;
  if (ta && !prev_touch_active && (nowMs - last_touch_ms) > 200) {
    last_touch_ms = nowMs;

    bool inStart = (last_touch_x >= BTN_START_X && last_touch_x <= (BTN_START_X + BTN_WIDTH) &&
                    last_touch_y >= BTN_Y && last_touch_y <= (BTN_Y + BTN_HEIGHT));
    bool inStop = (last_touch_x >= BTN_STOP_X && last_touch_x <= (BTN_STOP_X + BTN_WIDTH) &&
                   last_touch_y >= BTN_Y && last_touch_y <= (BTN_Y + BTN_HEIGHT));

    if (inStart && session_state == STATE_IDLE) {
      session_state = STATE_RUNNING;
      lap_count = 0;
      last_lap_ms = nowMs;              // anchor lap timing at session start
      last_lap_duration_ms = 0;
      best_lap_ms = 0;
      session_start_ms = nowMs;

      Serial.printf("SESSION_START @ %lu ms\n", (unsigned long)nowMs);
      Serial.println("CSV,lap,lap_ms,session_ms,best_ms");
    } else if (inStop && session_state == STATE_RUNNING) {
      session_state = STATE_IDLE;
      Serial.printf("SESSION_END @ %lu ms\n", (unsigned long)nowMs);
    }
  }
  prev_touch_active = ta;

  // IR sampling & state machine
  static uint32_t lastSampleMs = 0;
  static uint32_t lastFallsSnapshot = 0;

  static bool beaconPresent = false;
  static uint8_t onStreak = 0;
  static uint8_t offStreak = 0;

  static uint16_t fallsLast = 0;
  static uint32_t lastLowAgeMs = 999999;

  // Debounce timestamp for beam transitions (keep separate from lap timing)
  static uint32_t last_trigger_ms = 0;

  if (nowMs - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = nowMs;

    uint32_t falls = 0, lastFallUs = 0;
    noInterrupts();
    falls = gFallingCount;
    lastFallUs = gLastFallingUs;
    interrupts();

    uint32_t fallsDelta = falls - lastFallsSnapshot;
    lastFallsSnapshot = falls;

    fallsLast = (fallsDelta > 65535) ? 65535 : (uint16_t)fallsDelta;
    if (lastFallUs != 0) lastLowAgeMs = (uint32_t)((micros() - lastFallUs) / 1000UL);

    // Update streaks
    if (fallsLast >= ON_THRESHOLD) {
      onStreak = (onStreak < 255) ? (onStreak + 1) : 255;
    } else {
      onStreak = 0;
    }

    if (fallsLast <= OFF_THRESHOLD) {
      offStreak = (offStreak < 255) ? (offStreak + 1) : 255;
    } else {
      offStreak = 0;
    }

    bool lastPresent = beaconPresent;

    // Hysteresis with persistence
    if (!beaconPresent && onStreak >= STREAK_N) beaconPresent = true;
    if (beaconPresent && offStreak >= STREAK_N) beaconPresent = false;

    // Lap on absent->present
    if (beaconPresent && !lastPresent) {
      uint32_t sinceTrigger = (last_trigger_ms == 0) ? 999999UL : (nowMs - last_trigger_ms);

      if (sinceTrigger < LAP_COOLDOWN_MS) {
        Serial.printf("IGNORE @ %lu ms (debounce) falls=%u\n",
                      (unsigned long)nowMs, (unsigned)fallsLast);
      } else {
        // If we're not running, don't let idle IR activity affect debounce timing.
        if (session_state != STATE_RUNNING) {
          Serial.printf("IGNORE @ %lu ms (state) falls=%u\n",
                        (unsigned long)nowMs, (unsigned)fallsLast);
        } else {
          // Running: update debounce timestamp and evaluate lap timing.
          last_trigger_ms = nowMs;

          uint32_t lap_time_ms = nowMs - last_lap_ms; // last_lap_ms set at session start + each accepted lap
          if (lap_time_ms < MIN_LAP_MS) {
            Serial.printf("IGNORE @ %lu ms (minlap) falls=%u lap=%lu\n",
                          (unsigned long)nowMs,
                          (unsigned)fallsLast,
                          (unsigned long)lap_time_ms);
          } else {
            lap_count++;
            last_lap_ms = nowMs;
            last_lap_duration_ms = lap_time_ms;

            if (best_lap_ms == 0 || lap_time_ms < best_lap_ms) {
              best_lap_ms = lap_time_ms;
            }

            Serial.printf("LAP %lu @ %lu ms (falls=%u)\n",
                          (unsigned long)lap_count, (unsigned long)nowMs, (unsigned)fallsLast);
            Serial.printf("CSV,%lu,%lu,%lu,%lu\n",
                          (unsigned long)lap_count,
                          (unsigned long)lap_time_ms,
                          (unsigned long)(nowMs - session_start_ms),
                          (unsigned long)best_lap_ms);
          }
        }
      }
    }

    // UI update at sample rate
    DrawUI(beaconPresent, fallsLast, onStreak, offStreak, lastLowAgeMs);
  }
}
