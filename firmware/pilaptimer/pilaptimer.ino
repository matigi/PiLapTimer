// ==== Milestone 4 known-good config ====
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
  STATE_ARMED = 1,
  STATE_RUNNING = 2,
  STATE_FINISHED = 3,
  STATE_STATS = 4,
};

static SessionState session_state = STATE_IDLE;

static const uint8_t DRIVER_MIN = 1;
static const uint8_t DRIVER_MAX = 10;
static const uint8_t LAP_MIN = 1;
static const uint8_t LAP_MAX = 99;

static uint8_t selected_driver = 1;
static uint8_t target_laps = 5;

struct DriverRun {
  bool valid;
  uint8_t laps;
  uint32_t total_ms;
  uint32_t best_ms;
  uint32_t avg_ms;
};

static DriverRun driver_runs[DRIVER_MAX] = {};
static int8_t last_completed_driver = -1;

// Lap state
static uint32_t lap_count = 0;
static uint32_t last_lap_ms = 0;               // anchor for lap timing (set on session start + each accepted lap)
static uint32_t last_lap_duration_ms = 0;
static uint32_t best_lap_ms = 0;
static uint32_t session_start_ms = 0;
static uint32_t session_total_ms = 0;
static uint32_t session_avg_ms = 0;

static const uint8_t BUZZER_PIN = 7;
static const uint16_t BUZZER_FREQ_HZ = 2000;

static bool buzzer_on = false;
static uint8_t buzzer_pulses_remaining = 0;
static uint16_t buzzer_on_ms = 0;
static uint16_t buzzer_off_ms = 0;
static uint32_t buzzer_next_ms = 0;

static const uint16_t BTN_SMALL_W = 36;
static const uint16_t BTN_SMALL_H = 30;
static const uint16_t BTN_BIG_W = 200;
static const uint16_t BTN_BIG_H = 44;
static const uint16_t BTN_ROW1_Y = 36;
static const uint16_t BTN_ROW2_Y = 76;
static const uint16_t BTN_DRIVER_MINUS_X = 168;
static const uint16_t BTN_DRIVER_PLUS_X = 224;
static const uint16_t BTN_LAPS_MINUS_X = 168;
static const uint16_t BTN_LAPS_PLUS_X = 224;

static const uint16_t BTN_PRIMARY_X = 40;
static const uint16_t BTN_PRIMARY_Y = 190;

static const uint16_t BTN_SECONDARY_W = 120;
static const uint16_t BTN_SECONDARY_H = 40;
static const uint16_t BTN_VIEW_X = 16;
static const uint16_t BTN_DONE_X = 144;
static const uint16_t BTN_RESULTS_Y = 210;

static const uint16_t BTN_BACK_X = 80;
static const uint16_t BTN_BACK_Y = 210;

// IR ISR counters
volatile uint32_t gFallingCount = 0;
volatile uint32_t gLastFallingUs = 0;

void IRAM_ATTR irFallingISR() {
  gFallingCount++;
  gLastFallingUs = (uint32_t)micros();
}

const char* SessionStateLabel(SessionState state) {
  switch (state) {
    case STATE_IDLE:
      return "IDLE";
    case STATE_ARMED:
      return "ARMED";
    case STATE_RUNNING:
      return "RUNNING";
    case STATE_FINISHED:
      return "FINISHED";
    case STATE_STATS:
      return "STATS";
    default:
      return "UNKNOWN";
  }
}

bool IsValidLapTarget(uint8_t laps) {
  return (laps >= LAP_MIN && laps <= LAP_MAX);
}

void FormatTimeMs(uint32_t ms, char* out, size_t out_len) {
  uint32_t minutes = ms / 60000UL;
  uint32_t seconds = (ms / 1000UL) % 60UL;
  uint32_t millis = ms % 1000UL;
  snprintf(out, out_len, "%lu:%02lu.%03lu",
           (unsigned long)minutes,
           (unsigned long)seconds,
           (unsigned long)millis);
}

void BuzzerOff() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

void BuzzerOn() {
  tone(BUZZER_PIN, BUZZER_FREQ_HZ);
}

void BuzzerStartPattern(uint8_t pulses, uint16_t on_ms, uint16_t off_ms) {
  buzzer_pulses_remaining = pulses;
  buzzer_on_ms = on_ms;
  buzzer_off_ms = off_ms;
  buzzer_on = false;
  BuzzerOff();
  buzzer_next_ms = millis();
}

void UpdateBuzzer(uint32_t nowMs) {
  if (buzzer_pulses_remaining == 0 && !buzzer_on) {
    return;
  }

  if (buzzer_next_ms != 0 && nowMs < buzzer_next_ms) {
    return;
  }

  if (buzzer_on) {
    BuzzerOff();
    buzzer_on = false;
    if (buzzer_pulses_remaining > 0) {
      buzzer_next_ms = nowMs + buzzer_off_ms;
    } else {
      buzzer_next_ms = 0;
    }
  } else {
    if (buzzer_pulses_remaining == 0) {
      buzzer_next_ms = 0;
      return;
    }
    BuzzerOn();
    buzzer_on = true;
    buzzer_pulses_remaining--;
    buzzer_next_ms = nowMs + buzzer_on_ms;
  }
}

void DrawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active) {
  UWORD border = active ? GREEN : WHITE;
  UWORD text = active ? GREEN : WHITE;
  Paint_DrawRectangle(x, y, x + w, y + h, border, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawString_EN(x + 8, y + (h / 2) - 8, label, &Font16, text, BLACK);
}

bool TouchInRect(uint16_t tx, uint16_t ty, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  return (tx >= x && tx <= (x + w) && ty >= y && ty <= (y + h));
}

void DrawIdleScreen() {
  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);

  char driver_text[32];
  snprintf(driver_text, sizeof(driver_text), "Driver: %u", (unsigned)selected_driver);
  Paint_DrawString_EN(8, BTN_ROW1_Y + 4, driver_text, &Font20, GREEN, BLACK);

  DrawButton(BTN_DRIVER_MINUS_X, BTN_ROW1_Y, BTN_SMALL_W, BTN_SMALL_H, "-", true);
  DrawButton(BTN_DRIVER_PLUS_X, BTN_ROW1_Y, BTN_SMALL_W, BTN_SMALL_H, "+", true);

  char lap_text[32];
  snprintf(lap_text, sizeof(lap_text), "Laps: %u", (unsigned)target_laps);
  Paint_DrawString_EN(8, BTN_ROW2_Y + 4, lap_text, &Font20, GREEN, BLACK);
  DrawButton(BTN_LAPS_MINUS_X, BTN_ROW2_Y, BTN_SMALL_W, BTN_SMALL_H, "-", true);
  DrawButton(BTN_LAPS_PLUS_X, BTN_ROW2_Y, BTN_SMALL_W, BTN_SMALL_H, "+", true);

  Paint_DrawString_EN(8, 118, "Last Run:", &Font16, WHITE, BLACK);
  const DriverRun& run = driver_runs[selected_driver - 1];
  if (run.valid) {
    char total_text[32];
    char best_text[32];
    FormatTimeMs(run.total_ms, total_text, sizeof(total_text));
    FormatTimeMs(run.best_ms, best_text, sizeof(best_text));
    char total_line[48];
    char best_line[48];
    snprintf(total_line, sizeof(total_line), "Total: %s", total_text);
    snprintf(best_line, sizeof(best_line), "Best:  %s", best_text);
    Paint_DrawString_EN(8, 138, total_line, &Font16, WHITE, BLACK);
    Paint_DrawString_EN(8, 158, best_line, &Font16, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(8, 138, "Total: --", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(8, 158, "Best:  --", &Font16, WHITE, BLACK);
  }

  DrawButton(BTN_PRIMARY_X, BTN_PRIMARY_Y, BTN_BIG_W, BTN_BIG_H, "START RUN", IsValidLapTarget(target_laps));
}

void DrawArmedScreen() {
  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);

  char driver_text[32];
  snprintf(driver_text, sizeof(driver_text), "Driver: %u", (unsigned)selected_driver);
  Paint_DrawString_EN(8, 40, driver_text, &Font20, GREEN, BLACK);

  char lap_text[32];
  snprintf(lap_text, sizeof(lap_text), "Target: %u laps", (unsigned)target_laps);
  Paint_DrawString_EN(8, 70, lap_text, &Font20, GREEN, BLACK);

  Paint_DrawString_EN(8, 120, "READY", &Font24, WHITE, BLACK);
  Paint_DrawString_EN(8, 150, "Cross start line", &Font16, WHITE, BLACK);
  Paint_DrawString_EN(8, 168, "to begin", &Font16, WHITE, BLACK);

  DrawButton(BTN_PRIMARY_X, BTN_PRIMARY_Y, BTN_BIG_W, BTN_BIG_H, "CANCEL", true);
}

void DrawRunningScreen(uint32_t nowMs) {
  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);

  char driver_text[32];
  snprintf(driver_text, sizeof(driver_text), "Driver: %u", (unsigned)selected_driver);
  Paint_DrawString_EN(8, 34, driver_text, &Font20, GREEN, BLACK);

  char lap_text[32];
  snprintf(lap_text, sizeof(lap_text), "Lap: %lu / %u", (unsigned long)lap_count, (unsigned)target_laps);
  Paint_DrawString_EN(8, 64, lap_text, &Font20, GREEN, BLACK);

  char last_text[32];
  if (last_lap_duration_ms > 0) {
    char formatted[24];
    FormatTimeMs(last_lap_duration_ms, formatted, sizeof(formatted));
    snprintf(last_text, sizeof(last_text), "Last: %s", formatted);
  } else {
    snprintf(last_text, sizeof(last_text), "Last: --");
  }
  Paint_DrawString_EN(8, 96, last_text, &Font16, WHITE, BLACK);

  char best_text[32];
  if (best_lap_ms > 0) {
    char formatted[24];
    FormatTimeMs(best_lap_ms, formatted, sizeof(formatted));
    snprintf(best_text, sizeof(best_text), "Best: %s", formatted);
  } else {
    snprintf(best_text, sizeof(best_text), "Best: --");
  }
  Paint_DrawString_EN(8, 118, best_text, &Font16, WHITE, BLACK);

  char session_text[32];
  char formatted[24];
  FormatTimeMs(nowMs - session_start_ms, formatted, sizeof(formatted));
  snprintf(session_text, sizeof(session_text), "Session: %s", formatted);
  Paint_DrawString_EN(8, 150, session_text, &Font16, WHITE, BLACK);
}

void DrawFinishedScreen() {
  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);

  char driver_text[32];
  snprintf(driver_text, sizeof(driver_text), "Driver: %u", (unsigned)selected_driver);
  Paint_DrawString_EN(8, 34, driver_text, &Font20, GREEN, BLACK);

  char lap_text[32];
  snprintf(lap_text, sizeof(lap_text), "Laps: %lu", (unsigned long)lap_count);
  Paint_DrawString_EN(8, 64, lap_text, &Font20, GREEN, BLACK);

  char total_text[24];
  char best_text[24];
  char avg_text[24];
  FormatTimeMs(session_total_ms, total_text, sizeof(total_text));
  FormatTimeMs(best_lap_ms, best_text, sizeof(best_text));
  FormatTimeMs(session_avg_ms, avg_text, sizeof(avg_text));

  char total_line[40];
  char best_line[40];
  char avg_line[40];
  snprintf(total_line, sizeof(total_line), "Total: %s", total_text);
  snprintf(best_line, sizeof(best_line), "Best:  %s", best_text);
  snprintf(avg_line, sizeof(avg_line), "Avg:   %s", avg_text);

  Paint_DrawString_EN(8, 100, total_line, &Font16, WHITE, BLACK);
  Paint_DrawString_EN(8, 122, best_line, &Font16, WHITE, BLACK);
  Paint_DrawString_EN(8, 144, avg_line, &Font16, WHITE, BLACK);

  DrawButton(BTN_VIEW_X, BTN_RESULTS_Y, BTN_SECONDARY_W, BTN_SECONDARY_H, "VIEW STATS", true);
  DrawButton(BTN_DONE_X, BTN_RESULTS_Y, BTN_SECONDARY_W, BTN_SECONDARY_H, "DONE", true);
}

void DrawStatsScreen() {
  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);
  char header[32];
  snprintf(header, sizeof(header), "Driver %u - Last Run", (unsigned)selected_driver);
  Paint_DrawString_EN(8, 34, header, &Font16, GREEN, BLACK);

  const DriverRun& current = driver_runs[selected_driver - 1];
  if (current.valid) {
    char total_text[24];
    char best_text[24];
    char avg_text[24];
    FormatTimeMs(current.total_ms, total_text, sizeof(total_text));
    FormatTimeMs(current.best_ms, best_text, sizeof(best_text));
    FormatTimeMs(current.avg_ms, avg_text, sizeof(avg_text));
    char total_line[40];
    char best_line[40];
    char avg_line[40];
    snprintf(total_line, sizeof(total_line), "Total: %s", total_text);
    snprintf(best_line, sizeof(best_line), "Best:  %s", best_text);
    snprintf(avg_line, sizeof(avg_line), "Avg:   %s", avg_text);
    Paint_DrawString_EN(8, 56, total_line, &Font16, WHITE, BLACK);
    Paint_DrawString_EN(8, 78, best_line, &Font16, WHITE, BLACK);
    Paint_DrawString_EN(8, 100, avg_line, &Font16, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(8, 56, "No runs yet.", &Font16, WHITE, BLACK);
  }

  Paint_DrawString_EN(8, 130, "Previous Driver:", &Font16, WHITE, BLACK);
  if (last_completed_driver >= 0 && last_completed_driver != (selected_driver - 1)) {
    const DriverRun& prev = driver_runs[last_completed_driver];
    char prev_header[24];
    snprintf(prev_header, sizeof(prev_header), "Driver %u", (unsigned)(last_completed_driver + 1));
    Paint_DrawString_EN(8, 150, prev_header, &Font16, GREEN, BLACK);
    if (prev.valid) {
      char best_text[24];
      FormatTimeMs(prev.best_ms, best_text, sizeof(best_text));
      char best_line[32];
      snprintf(best_line, sizeof(best_line), "Best: %s", best_text);
      Paint_DrawString_EN(8, 170, best_line, &Font16, WHITE, BLACK);
    }
  } else {
    Paint_DrawString_EN(8, 150, "None", &Font16, WHITE, BLACK);
  }

  DrawButton(BTN_BACK_X, BTN_BACK_Y, BTN_SECONDARY_W, BTN_SECONDARY_H, "BACK", true);
}

void DrawUI(bool beaconPresent, uint16_t fallsLast, uint8_t onStreak, uint8_t offStreak, uint32_t lastLowAgeMs, uint32_t nowMs) {
  if (!gFrame) return;

  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  switch (session_state) {
    case STATE_IDLE:
      DrawIdleScreen();
      break;
    case STATE_ARMED:
      DrawArmedScreen();
      break;
    case STATE_RUNNING:
      DrawRunningScreen(nowMs);
      break;
    case STATE_FINISHED:
      DrawFinishedScreen();
      break;
    case STATE_STATS:
      DrawStatsScreen();
      break;
  }

  char state_text[32];
  snprintf(state_text, sizeof(state_text), "State: %s", SessionStateLabel(session_state));
  Paint_DrawString_EN(8, 260, state_text, &Font16, WHITE, BLACK);

  char ir1[64];
  snprintf(ir1, sizeof(ir1), "IR pin=%u falls=%u/%lums", (unsigned)IR_IN_PIN, (unsigned)fallsLast, (unsigned long)SAMPLE_MS);
  Paint_DrawString_EN(8, 280, ir1, &Font16, WHITE, BLACK);

  char ir2[64];
  snprintf(ir2, sizeof(ir2), "Beacon=%s on=%u off=%u",
           beaconPresent ? "YES" : "NO",
           (unsigned)onStreak, (unsigned)offStreak);
  Paint_DrawString_EN(8, 300, ir2, &Font16, beaconPresent ? GREEN : RED, BLACK);

  char ir3[64];
  snprintf(ir3, sizeof(ir3), "lastLOW=%lums", (unsigned long)lastLowAgeMs);
  Paint_DrawString_EN(8, 320, ir3, &Font16, WHITE, BLACK);

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
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.printf("IR pin=%u, sample=%lums, on>=%u, off<=%u, streak=%u\n",
                (unsigned)IR_IN_PIN, (unsigned long)SAMPLE_MS,
                (unsigned)ON_THRESHOLD, (unsigned)OFF_THRESHOLD, (unsigned)STREAK_N);

  DrawUI(false, 0, 0, 0, 999999, millis());
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

    if (session_state != STATE_RUNNING) {
      bool inDriverMinus = TouchInRect(last_touch_x, last_touch_y, BTN_DRIVER_MINUS_X, BTN_ROW1_Y, BTN_SMALL_W, BTN_SMALL_H);
      bool inDriverPlus = TouchInRect(last_touch_x, last_touch_y, BTN_DRIVER_PLUS_X, BTN_ROW1_Y, BTN_SMALL_W, BTN_SMALL_H);
      bool inLapsMinus = TouchInRect(last_touch_x, last_touch_y, BTN_LAPS_MINUS_X, BTN_ROW2_Y, BTN_SMALL_W, BTN_SMALL_H);
      bool inLapsPlus = TouchInRect(last_touch_x, last_touch_y, BTN_LAPS_PLUS_X, BTN_ROW2_Y, BTN_SMALL_W, BTN_SMALL_H);
      bool inPrimary = TouchInRect(last_touch_x, last_touch_y, BTN_PRIMARY_X, BTN_PRIMARY_Y, BTN_BIG_W, BTN_BIG_H);
      bool inView = TouchInRect(last_touch_x, last_touch_y, BTN_VIEW_X, BTN_RESULTS_Y, BTN_SECONDARY_W, BTN_SECONDARY_H);
      bool inDone = TouchInRect(last_touch_x, last_touch_y, BTN_DONE_X, BTN_RESULTS_Y, BTN_SECONDARY_W, BTN_SECONDARY_H);
      bool inBack = TouchInRect(last_touch_x, last_touch_y, BTN_BACK_X, BTN_BACK_Y, BTN_SECONDARY_W, BTN_SECONDARY_H);

      if (session_state == STATE_IDLE) {
        if (inDriverMinus) {
          selected_driver = (selected_driver == DRIVER_MIN) ? DRIVER_MAX : (selected_driver - 1);
        } else if (inDriverPlus) {
          selected_driver = (selected_driver == DRIVER_MAX) ? DRIVER_MIN : (selected_driver + 1);
        } else if (inLapsMinus) {
          if (target_laps > LAP_MIN) target_laps--;
        } else if (inLapsPlus) {
          if (target_laps < LAP_MAX) target_laps++;
        } else if (inPrimary && IsValidLapTarget(target_laps)) {
          session_state = STATE_ARMED;
          Serial.printf("SESSION_ARMED driver=%u target_laps=%u\n",
                        (unsigned)selected_driver, (unsigned)target_laps);
        }
      } else if (session_state == STATE_ARMED) {
        if (inPrimary) {
          session_state = STATE_IDLE;
          Serial.println("SESSION_CANCEL");
        }
      } else if (session_state == STATE_FINISHED) {
        if (inView) {
          session_state = STATE_STATS;
        } else if (inDone) {
          session_state = STATE_IDLE;
        }
      } else if (session_state == STATE_STATS) {
        if (inBack) {
          session_state = STATE_FINISHED;
        }
      }
    }
  }
  prev_touch_active = ta;

  UpdateBuzzer(nowMs);

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
      } else if (session_state == STATE_ARMED) {
        session_state = STATE_RUNNING;
        lap_count = 0;
        last_lap_ms = nowMs;
        last_lap_duration_ms = 0;
        best_lap_ms = 0;
        session_start_ms = nowMs;
        session_total_ms = 0;
        session_avg_ms = 0;
        last_trigger_ms = nowMs;

        Serial.printf("SESSION_START @ %lu ms driver=%u target=%u\n",
                      (unsigned long)nowMs,
                      (unsigned)selected_driver,
                      (unsigned)target_laps);
        Serial.println("CSV,lap,lap_ms,session_ms,best_ms");
      } else if (session_state == STATE_RUNNING) {
        if (lap_count >= target_laps) {
          Serial.printf("IGNORE @ %lu ms (complete) falls=%u\n",
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

            if (lap_count >= target_laps) {
              session_total_ms = nowMs - session_start_ms;
              session_avg_ms = (lap_count > 0) ? (session_total_ms / lap_count) : 0;

              DriverRun& run = driver_runs[selected_driver - 1];
              run.valid = true;
              run.laps = lap_count;
              run.total_ms = session_total_ms;
              run.best_ms = best_lap_ms;
              run.avg_ms = session_avg_ms;
              last_completed_driver = selected_driver - 1;

              BuzzerStartPattern(2, 100, 100);
              session_state = STATE_FINISHED;
              Serial.printf("SESSION_END @ %lu ms total=%lu best=%lu avg=%lu\n",
                            (unsigned long)nowMs,
                            (unsigned long)session_total_ms,
                            (unsigned long)best_lap_ms,
                            (unsigned long)session_avg_ms);
            } else {
              BuzzerStartPattern(1, 80, 60);
            }
          }
        }
      } else {
        Serial.printf("IGNORE @ %lu ms (state) falls=%u\n",
                      (unsigned long)nowMs, (unsigned)fallsLast);
      }
    }

    // UI update at sample rate
    DrawUI(beaconPresent, fallsLast, onStreak, offStreak, lastLowAgeMs, nowMs);
  }
}
