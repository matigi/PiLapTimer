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

// ===== Trigger tuning (start here) =====
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

// Lap state
static uint32_t lap_count = 0;
static uint32_t last_lap_ms = 0;

// IR ISR counters
volatile uint32_t gFallingCount = 0;
volatile uint32_t gLastFallingUs = 0;

void IRAM_ATTR irFallingISR() {
  gFallingCount++;
  gLastFallingUs = (uint32_t)micros();
}

void DrawUI(bool beaconPresent, uint16_t fallsLast, uint8_t onStreak, uint8_t offStreak, uint32_t lastLowAgeMs) {
  if (!gFrame) return;

  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);

  char lap_text[32];
  snprintf(lap_text, sizeof(lap_text), "Lap: %lu", (unsigned long)lap_count);
  Paint_DrawString_EN(8, 32, lap_text, &Font20, GREEN, BLACK);

  if (last_touch_active) {
    char t[48];
    snprintf(t, sizeof(t), "Touch %u,%u", last_touch_x, last_touch_y);
    Paint_DrawString_EN(8, 60, t, &Font16, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(8, 60, "No touch", &Font16, RED, BLACK);
  }

  char ir1[64];
  snprintf(ir1, sizeof(ir1), "IR pin=%u falls=%u/%lums", (unsigned)IR_IN_PIN, (unsigned)fallsLast, (unsigned long)SAMPLE_MS);
  Paint_DrawString_EN(8, 88, ir1, &Font16, WHITE, BLACK);

  char ir2[64];
  snprintf(ir2, sizeof(ir2), "Beacon=%s on=%u off=%u",
           beaconPresent ? "YES" : "NO",
           (unsigned)onStreak, (unsigned)offStreak);
  Paint_DrawString_EN(8, 108, ir2, &Font16, beaconPresent ? GREEN : RED, BLACK);

  char ir3[64];
  snprintf(ir3, sizeof(ir3), "lastLOW=%lums", (unsigned long)lastLowAgeMs);
  Paint_DrawString_EN(8, 128, ir3, &Font16, WHITE, BLACK);

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

  // IR sampling & state machine
  static uint32_t lastSampleMs = 0;
  static uint32_t lastFallsSnapshot = 0;

  static bool beaconPresent = false;
  static uint8_t onStreak = 0;
  static uint8_t offStreak = 0;

  static uint16_t fallsLast = 0;
  static uint32_t lastLowAgeMs = 999999;

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
      uint32_t sinceLast = (last_lap_ms == 0) ? 999999UL : (nowMs - last_lap_ms);

      if (sinceLast >= LAP_COOLDOWN_MS && sinceLast >= MIN_LAP_MS) {
        lap_count++;
        last_lap_ms = nowMs;
        Serial.printf("LAP %lu @ %lu ms (falls=%u)\n",
                      (unsigned long)lap_count, (unsigned long)nowMs, (unsigned)fallsLast);
      } else {
        Serial.printf("IGNORE @ %lu ms (cooldown/minlap) falls=%u\n",
                      (unsigned long)nowMs, (unsigned)fallsLast);
      }
    }

    // UI update at sample rate
    DrawUI(beaconPresent, fallsLast, onStreak, offStreak, lastLowAgeMs);
  }
}
