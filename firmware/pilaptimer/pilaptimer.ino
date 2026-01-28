#include <Arduino.h>
#include <string.h>

#include "DEV_Config.h"
#include "AMOLED_1in64.h"
#include "qspi_pio.h"
#include "QMI8658.h"
#include "FT3168.h"
#include "GUI_Paint.h"
#include "fonts.h"
#include "ui_state.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 1
#endif

#if USE_LVGL_UI
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_time_attack_ui.h"
#endif

#ifndef WHITE
#define WHITE 0xFFFF
#define BLACK 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define BLUE 0x001F
#endif

static const uint16_t DISP_W = 280;
static const uint16_t DISP_H = 456;
static const uint16_t LOGICAL_W = DISP_H;
static const uint16_t LOGICAL_H = DISP_W;
static const UWORD UI_ROTATION = ROTATE_270;

#ifndef TOUCH_DEBUG
#define TOUCH_DEBUG 0
#endif

// IMPORTANT: Do NOT use GP6/GP7 (I2C). Use GP16 (wire buzzer + to GP16, - to GND).
static const uint8_t  BUZZER_PIN = 16;

// IR lap trigger input
static const uint8_t  IR_IN_PIN = 1; // GP1
static const uint32_t LAP_LOCKOUT_MS = 1500;

// Polling + state windowing
static const uint16_t POLL_MS              = 10;
static const uint16_t TOUCH_HOLD_MS        = 120;
static const uint16_t BEEP_DEBOUNCE_MS     = 250;
static const uint16_t UI_REFRESH_MS        = 250;
static const uint16_t LVGL_UI_REFRESH_MS   = 100;

static UWORD* gFrame = nullptr;

// ----------------- UI helpers -----------------
struct Button {
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
  const char* label;
};

// ----------------- UI layout constants -----------------
static const uint16_t UI_MARGIN = 12;
static const uint16_t UI_GAP = 10;
static const uint16_t UI_COL_GAP = 12;
static const uint16_t UI_LEFT_COL_W = 220;
static const uint16_t UI_RIGHT_COL_W = LOGICAL_W - (UI_MARGIN * 2) - UI_COL_GAP - UI_LEFT_COL_W;
static const uint16_t UI_HEADER_H = 28;
static const uint16_t UI_ROW_H = 70;
static const uint16_t UI_STEP_BTN = 52;
static const uint16_t UI_START_H = 56;
static const uint16_t UI_BUTTON_H = 52;

static const uint16_t UI_LEFT_X = UI_MARGIN;
static const uint16_t UI_RIGHT_X = UI_LEFT_X + UI_LEFT_COL_W + UI_COL_GAP;
static const uint16_t UI_HEADER_Y = UI_MARGIN;
static const uint16_t UI_SECTION_Y = UI_HEADER_Y + UI_HEADER_H + UI_GAP;
static const uint16_t UI_DRIVER_Y = UI_SECTION_Y;
static const uint16_t UI_LAPS_Y = UI_DRIVER_Y + UI_ROW_H + UI_GAP;
static const uint16_t UI_START_Y = LOGICAL_H - UI_MARGIN - UI_START_H;

static const uint16_t UI_STEP_MINUS_X = UI_LEFT_X + UI_LEFT_COL_W - (UI_STEP_BTN * 2 + UI_GAP);
static const uint16_t UI_STEP_PLUS_X = UI_LEFT_X + UI_LEFT_COL_W - UI_STEP_BTN;
static const uint16_t UI_STEP_Y_OFFSET = (UI_ROW_H - UI_STEP_BTN) / 2;

static const uint16_t UI_BUTTON_PAIR_W = 150;
static const uint16_t UI_BUTTON_PAIR_GAP = 12;
static const uint16_t UI_BUTTON_PAIR_X = (LOGICAL_W - (UI_BUTTON_PAIR_W * 2 + UI_BUTTON_PAIR_GAP)) / 2;
static const uint16_t UI_BUTTON_PAIR_Y = LOGICAL_H - UI_MARGIN - UI_BUTTON_H;

static const uint16_t UI_CENTER_BUTTON_W = 180;
static const uint16_t UI_CENTER_BUTTON_X = (LOGICAL_W - UI_CENTER_BUTTON_W) / 2;

// ----------------- Beep (no tone) -----------------
static void BeepNow(uint16_t freq = 2600, uint16_t ms = 25) {
  analogWriteFreq(freq);
  analogWrite(BUZZER_PIN, 128);
  delay(ms);
  analogWrite(BUZZER_PIN, 0);
}

static void BeepLap() {
  BeepNow(2600, 25);
}

static void BeepComplete() {
  BeepNow(2200, 60);
  delay(40);
  BeepNow(2800, 60);
}

// ----------------- Touch helpers -----------------
static inline bool TouchLooksInvalid(uint16_t x, uint16_t y) {
  if (x == 4095 && y == 4095) return true;
  if (x == 0xFFFF && y == 0xFFFF) return true;
  return false;
}

static bool ReadTouchSample(uint16_t &rawX, uint16_t &rawY) {
  if (!FT3168_Get_Point()) return false;

  uint16_t rx = (uint16_t)FT3168.x_point;
  uint16_t ry = (uint16_t)FT3168.y_point;
  if (TouchLooksInvalid(rx, ry)) return false;

  rawX = rx;
  rawY = ry;
  return true;
}

static void NormalizeTouch(uint16_t rawX, uint16_t rawY, uint16_t &nx, uint16_t &ny) {
  uint32_t x = rawX;
  uint32_t y = rawY;

  if (rawX > (DISP_W + 50) || rawY > (DISP_H + 50)) {
    x = (uint32_t)rawX * (DISP_W - 1) / 4095UL;
    y = (uint32_t)rawY * (DISP_H - 1) / 4095UL;
  }

  if (x >= DISP_W) x = DISP_W - 1;
  if (y >= DISP_H) y = DISP_H - 1;

  nx = (uint16_t)x;
  ny = (uint16_t)y;
}

static void RotateTouch(uint16_t nx, uint16_t ny, uint16_t &rx, uint16_t &ry) {
  rx = (LOGICAL_W - 1) - ny;
  ry = nx;
}

// ----------------- IR lap trigger -----------------
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

volatile bool gIrSeen = false;
volatile uint32_t gIrSeenMs = 0;

void IRAM_ATTR IrIsr() {
  gIrSeenMs = (uint32_t)millis();
  gIrSeen = true;
}

static bool HitTest(const Button &btn, uint16_t x, uint16_t y) {
  return (x >= btn.x && x < (btn.x + btn.w) && y >= btn.y && y < (btn.y + btn.h));
}

static uint16_t TextWidth(const char* text, const sFONT* font) {
  return (uint16_t)(strlen(text) * font->Width);
}

static void DrawCenteredText(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* text,
                             const sFONT* font, uint16_t textColor, uint16_t bgColor) {
  uint16_t textW = TextWidth(text, font);
  uint16_t textH = font->Height;
  uint16_t textX = x + (w > textW ? (w - textW) / 2 : 0);
  uint16_t textY = y + (h > textH ? (h - textH) / 2 : 0);
  Paint_DrawString_EN(textX, textY, text, (sFONT*)font, textColor, bgColor);
}

static void DrawButton(const Button &btn, const sFONT* font, uint16_t textColor, uint16_t fillColor, uint16_t borderColor = WHITE) {
  Paint_DrawRectangle(btn.x, btn.y, btn.x + btn.w, btn.y + btn.h, fillColor, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawRectangle(btn.x, btn.y, btn.x + btn.w, btn.y + btn.h, borderColor, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  DrawCenteredText(btn.x, btn.y, btn.w, btn.h, btn.label, font, textColor, fillColor);
}

static void DrawHeader(const char* title) {
  Paint_DrawString_EN(UI_MARGIN, UI_HEADER_Y, title, &Font20, WHITE, BLACK);
}

static void DrawValueBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* value) {
  Paint_DrawRectangle(x, y, x + w, y + h, DARKBLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawRectangle(x, y, x + w, y + h, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  DrawCenteredText(x, y, w, h, value, &Font24, WHITE, DARKBLUE);
}

static void FormatTime(char *out, size_t outSize, uint32_t ms) {
  if (outSize == 0) return;
  uint32_t totalSec = ms / 1000;
  uint32_t minutes = totalSec / 60;
  uint32_t seconds = totalSec % 60;
  uint32_t millisPart = ms % 1000;
  snprintf(out, outSize, "%lu:%02lu.%03lu",
           (unsigned long)minutes,
           (unsigned long)seconds,
           (unsigned long)millisPart);
}

static void FormatTimeMaybe(char *out, size_t outSize, bool valid, uint32_t ms) {
  if (!valid) {
    snprintf(out, outSize, "--:--.---");
    return;
  }
  FormatTime(out, outSize, ms);
}

// ----------------- State -----------------
static UiState gState = UI_BOOT;

struct RunStats {
  bool valid;
  uint32_t totalMs;
  uint32_t bestMs;
  uint32_t avgMs;
  uint16_t laps;
};

static const uint8_t MAX_DRIVERS = 10;
static const uint8_t MIN_LAPS = 1;
static const uint8_t MAX_LAPS = 20;

static RunStats gDriverRuns[MAX_DRIVERS] = {};
static int gLastCompletedDriver = -1;

static uint8_t gSelectedDriver = 1;
static uint8_t gSelectedLaps = 5;

static uint8_t gLapCount = 0;
static uint32_t gStartMs = 0;
static uint32_t gLastLapStartMs = 0;
static uint32_t gLastLapMs = 0;
static uint32_t gBestLapMs = 0;
static int32_t gDeltaMs = 0;
static uint32_t gSessionMs = 0;
static uint32_t gLastUiMs = 0;
static uint32_t gLastLapTriggerMs = 0;
static uint32_t gLastBeepMs = 0;

#if USE_LVGL_UI
static bool gUiDirty = true;
static uint32_t gLastLvglUiMs = 0;
#endif
// Buttons (idle)
static const Button BTN_DRIVER_MINUS = {UI_STEP_MINUS_X, UI_DRIVER_Y + UI_STEP_Y_OFFSET, UI_STEP_BTN, UI_STEP_BTN, "-"};
static const Button BTN_DRIVER_PLUS  = {UI_STEP_PLUS_X, UI_DRIVER_Y + UI_STEP_Y_OFFSET, UI_STEP_BTN, UI_STEP_BTN, "+"};
static const Button BTN_LAP_MINUS    = {UI_STEP_MINUS_X, UI_LAPS_Y + UI_STEP_Y_OFFSET, UI_STEP_BTN, UI_STEP_BTN, "-"};
static const Button BTN_LAP_PLUS     = {UI_STEP_PLUS_X, UI_LAPS_Y + UI_STEP_Y_OFFSET, UI_STEP_BTN, UI_STEP_BTN, "+"};
static const Button BTN_START        = {UI_LEFT_X, UI_START_Y, UI_LEFT_COL_W, UI_START_H, "START RUN"};

// Buttons (armed)
static const Button BTN_CANCEL       = {UI_CENTER_BUTTON_X, LOGICAL_H - UI_MARGIN - UI_BUTTON_H, UI_CENTER_BUTTON_W, UI_BUTTON_H, "CANCEL"};

// Buttons (finished)
static const Button BTN_VIEW_STATS   = {UI_BUTTON_PAIR_X, UI_BUTTON_PAIR_Y, UI_BUTTON_PAIR_W, UI_BUTTON_H, "STATS"};
static const Button BTN_DONE         = {UI_BUTTON_PAIR_X + UI_BUTTON_PAIR_W + UI_BUTTON_PAIR_GAP, UI_BUTTON_PAIR_Y, UI_BUTTON_PAIR_W, UI_BUTTON_H, "DONE"};

// Buttons (stats)
static const Button BTN_BACK         = {UI_CENTER_BUTTON_X, LOGICAL_H - UI_MARGIN - UI_BUTTON_H, UI_CENTER_BUTTON_W, UI_BUTTON_H, "BACK"};

static void DrawSplash(const char* line2) {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);
  Paint_DrawString_EN(24, 160, "PiLapTimer", &Font24, BLACK, WHITE);
  Paint_DrawString_EN(18, 200, line2, &Font16, BLACK, GREEN);
  AMOLED_1IN64_Display(gFrame);
}

static void DrawIdleScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  DrawHeader("Time Attack");

  char line[48];
  Paint_DrawString_EN(UI_LEFT_X, UI_DRIVER_Y, "Driver", &Font16, WHITE, BLACK);
  snprintf(line, sizeof(line), "%u", (unsigned)gSelectedDriver);
  DrawValueBox(UI_LEFT_X, UI_DRIVER_Y + 26, 110, 36, line);

  Paint_DrawString_EN(UI_LEFT_X, UI_LAPS_Y, "Laps", &Font16, WHITE, BLACK);
  snprintf(line, sizeof(line), "%u", (unsigned)gSelectedLaps);
  DrawValueBox(UI_LEFT_X, UI_LAPS_Y + 26, 110, 36, line);

  DrawButton(BTN_DRIVER_MINUS, &Font24, WHITE, BLUE);
  DrawButton(BTN_DRIVER_PLUS, &Font24, WHITE, BLUE);
  DrawButton(BTN_LAP_MINUS, &Font24, WHITE, BLUE);
  DrawButton(BTN_LAP_PLUS, &Font24, WHITE, BLUE);

  DrawButton(BTN_START, &Font20, BLACK, GREEN);

  RunStats &run = gDriverRuns[gSelectedDriver - 1];
  Paint_DrawString_EN(UI_RIGHT_X, UI_SECTION_Y, "Last Run", &Font16, YELLOW, BLACK);

  char timeBuf[24];
  uint16_t infoY = UI_SECTION_Y + 22;
  if (run.valid) {
    FormatTime(timeBuf, sizeof(timeBuf), run.totalMs);
    snprintf(line, sizeof(line), "Total: %s", timeBuf);
    Paint_DrawString_EN(UI_RIGHT_X, infoY, line, &Font16, WHITE, BLACK);
    infoY += 22;

    FormatTime(timeBuf, sizeof(timeBuf), run.bestMs);
    snprintf(line, sizeof(line), "Best:  %s", timeBuf);
    Paint_DrawString_EN(UI_RIGHT_X, infoY, line, &Font16, WHITE, BLACK);
    infoY += 22;
  } else {
    Paint_DrawString_EN(UI_RIGHT_X, infoY, "No run recorded", &Font16, WHITE, BLACK);
  }

  AMOLED_1IN64_Display(gFrame);
}

static void DrawArmedScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[48];
  DrawHeader("Ready to Start");

  snprintf(line, sizeof(line), "Driver %u", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y, line, &Font16, WHITE, BLACK);

  snprintf(line, sizeof(line), "Target Laps: %u", (unsigned)gSelectedLaps);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 24, line, &Font16, WHITE, BLACK);

  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 70, "READY", &Font24, GREEN, BLACK);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 104, "Cross start line", &Font16, WHITE, BLACK);

  DrawButton(BTN_CANCEL, &Font20, WHITE, RED);

  AMOLED_1IN64_Display(gFrame);
}

static void DrawRunningScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[64];
  DrawHeader("Running");

  snprintf(line, sizeof(line), "Driver %u", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y, line, &Font16, WHITE, BLACK);

  uint8_t lapDisplay = (gLapCount < gSelectedLaps) ? (gLapCount + 1) : gSelectedLaps;
  snprintf(line, sizeof(line), "LAP %u / %u", (unsigned)lapDisplay, (unsigned)gSelectedLaps);
  DrawCenteredText(UI_MARGIN, UI_SECTION_Y + 20, LOGICAL_W - (UI_MARGIN * 2), 34, line, &Font24, YELLOW, BLACK);

  uint32_t now = millis();
  uint32_t currentLapMs = (gLapCount == 0) ? (now - gStartMs) : (now - gLastLapStartMs);
  char timeBuf[24];
  FormatTime(timeBuf, sizeof(timeBuf), currentLapMs);
  DrawValueBox(UI_MARGIN, UI_SECTION_Y + 56, LOGICAL_W - (UI_MARGIN * 2), 46, timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 108, "Current Lap", &Font16, WHITE, BLACK);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), gLapCount > 0, gLastLapMs);
  snprintf(line, sizeof(line), "Last: %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 132, line, &Font16, WHITE, BLACK);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), gBestLapMs > 0, gBestLapMs);
  snprintf(line, sizeof(line), "Best: %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 154, line, &Font16, WHITE, BLACK);

  if (gLapCount > 0 && gBestLapMs > 0) {
    long delta = (long)gDeltaMs;
    snprintf(line, sizeof(line), "Delta: %c%lu ms", (delta >= 0) ? '+' : '-', (unsigned long)labs(delta));
    Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 176, line, &Font16, (delta <= 0) ? GREEN : RED, BLACK);
  }

  FormatTime(timeBuf, sizeof(timeBuf), gSessionMs);
  snprintf(line, sizeof(line), "Session: %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 202, line, &Font16, WHITE, BLACK);

  Paint_DrawString_EN(UI_MARGIN, LOGICAL_H - UI_MARGIN - 18, "IR OK", &Font16, GREEN, BLACK);

  AMOLED_1IN64_Display(gFrame);
}

static void DrawFinishedScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[64];
  DrawHeader("Run Complete");

  RunStats &run = gDriverRuns[gSelectedDriver - 1];
  char timeBuf[24];

  snprintf(line, sizeof(line), "Driver %u", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y, line, &Font16, WHITE, BLACK);

  snprintf(line, sizeof(line), "Laps %u", (unsigned)gSelectedLaps);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 22, line, &Font16, WHITE, BLACK);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), run.valid, run.totalMs);
  DrawCenteredText(UI_MARGIN, UI_SECTION_Y + 52, LOGICAL_W - (UI_MARGIN * 2), 40, timeBuf, &Font24, WHITE, BLACK);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 96, "Total Time", &Font16, WHITE, BLACK);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), run.valid, run.bestMs);
  snprintf(line, sizeof(line), "Best:  %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 122, line, &Font16, WHITE, BLACK);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), run.valid, run.avgMs);
  snprintf(line, sizeof(line), "Avg:   %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 144, line, &Font16, WHITE, BLACK);

  DrawButton(BTN_VIEW_STATS, &Font20, WHITE, BLUE);
  DrawButton(BTN_DONE, &Font20, BLACK, GREEN);

  AMOLED_1IN64_Display(gFrame);
}

static void DrawStatsScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[64];
  RunStats &selected = gDriverRuns[gSelectedDriver - 1];
  DrawHeader("Run Stats");

  char timeBuf[24];
  FormatTimeMaybe(timeBuf, sizeof(timeBuf), selected.valid, selected.totalMs);
  snprintf(line, sizeof(line), "Total: %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y, line, &Font16, WHITE, BLACK);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), selected.valid, selected.bestMs);
  snprintf(line, sizeof(line), "Best:  %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 22, line, &Font16, WHITE, BLACK);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), selected.valid, selected.avgMs);
  snprintf(line, sizeof(line), "Avg:   %s", timeBuf);
  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 44, line, &Font16, WHITE, BLACK);

  Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 80, "Previous Driver", &Font16, YELLOW, BLACK);

  int compareDriver = gLastCompletedDriver;
  if (compareDriver == (int)gSelectedDriver) {
    compareDriver = -1;
  }

  if (compareDriver > 0) {
    RunStats &other = gDriverRuns[compareDriver - 1];
    snprintf(line, sizeof(line), "Driver %u", (unsigned)compareDriver);
    Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 102, line, &Font16, WHITE, BLACK);
    FormatTimeMaybe(timeBuf, sizeof(timeBuf), other.valid, other.bestMs);
    snprintf(line, sizeof(line), "Best: %s", timeBuf);
    Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 124, line, &Font16, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(UI_MARGIN, UI_SECTION_Y + 102, "No previous run", &Font16, WHITE, BLACK);
  }

  DrawButton(BTN_BACK, &Font20, WHITE, BLUE);

  AMOLED_1IN64_Display(gFrame);
}

static void RenderState() {
#if USE_LVGL_UI
  gUiDirty = true;
#else
  switch (gState) {
    case UI_IDLE:
      DrawIdleScreen();
      break;
    case UI_ARMED:
      DrawArmedScreen();
      break;
    case UI_RUNNING:
      DrawRunningScreen();
      break;
    case UI_FINISHED:
      DrawFinishedScreen();
      break;
    case UI_STATS:
      DrawStatsScreen();
      break;
    default:
      break;
  }
#endif
}

static void StartArmed() {
  gLapCount = 0;
  gLastLapMs = 0;
  gBestLapMs = 0;
  gDeltaMs = 0;
  gSessionMs = 0;
  gStartMs = 0;
  gLastLapStartMs = 0;
  gLastLapTriggerMs = 0;
  gState = UI_ARMED;
  RenderState();
}

static void StartRunning(uint32_t now) {
  gStartMs = now;
  gLastLapStartMs = now;
  gSessionMs = 0;
  gState = UI_RUNNING;
  gLastUiMs = 0;
  RenderState();
}

static void FinishRun(uint32_t now) {
  gSessionMs = now - gStartMs;

  RunStats &run = gDriverRuns[gSelectedDriver - 1];
  run.valid = true;
  run.totalMs = gSessionMs;
  run.bestMs = gBestLapMs;
  run.laps = gLapCount;
  run.avgMs = (gLapCount > 0) ? (gSessionMs / gLapCount) : 0;
  gLastCompletedDriver = gSelectedDriver;

  gState = UI_FINISHED;
  RenderState();
}

#if USE_LVGL_UI
static void ResetRunState() {
  gLapCount = 0;
  gLastLapMs = 0;
  gBestLapMs = 0;
  gDeltaMs = 0;
  gSessionMs = 0;
  gStartMs = 0;
  gLastLapStartMs = 0;
  gLastLapTriggerMs = 0;
}

static void HandleStartStop() {
  uint32_t now = millis();
  if (gState == UI_IDLE) {
    StartArmed();
    if ((now - gLastBeepMs) >= BEEP_DEBOUNCE_MS) {
      BeepNow(2400, 35);
      gLastBeepMs = now;
    }
    return;
  }

  if (gState == UI_ARMED || gState == UI_FINISHED || gState == UI_STATS) {
    gState = UI_IDLE;
    RenderState();
  }
}

static void HandleReset() {
  if (gState == UI_RUNNING) return;
  ResetRunState();
  gState = UI_IDLE;
  RenderState();
}
#endif

// ----------------- Arduino -----------------
void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println();
  Serial.println("BOOT: PiLapTimer time attack UI");

  if (DEV_Module_Init() != 0) Serial.println("DEV_Module_Init FAILED");
  else Serial.println("DEV_Module_Init OK");

  QSPI_GPIO_Init(qspi);
  QSPI_PIO_Init(qspi);
  QSPI_1Wrie_Mode(&qspi);

  AMOLED_1IN64_Init();
  AMOLED_1IN64_SetBrightness(100);
  AMOLED_1IN64_Clear(WHITE);

  Serial.printf("Display WIDTH=%u HEIGHT=%u\n", (unsigned)AMOLED_1IN64.WIDTH, (unsigned)AMOLED_1IN64.HEIGHT);

#if !USE_LVGL_UI
  UDOUBLE bytes = (UDOUBLE)DISP_W * (UDOUBLE)DISP_H * 2;
  gFrame = (UWORD*)malloc(bytes);
  if (!gFrame) {
    Serial.println("FATAL: framebuffer malloc failed");
    while (true) delay(1000);
  }

  Paint_NewImage((UBYTE*)gFrame, DISP_W, DISP_H, ROTATE_0, WHITE);
  Paint_SetScale(65);
  Paint_SetRotate(ROTATE_0);
#else
  AMOLED_1IN64_Clear(BLACK);
#endif

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.printf("BUZZER: test on pin %u\n", (unsigned)BUZZER_PIN);
  BeepNow();
  delay(200);
  BeepNow();

#if !USE_LVGL_UI
  DrawSplash("Booting...");
  delay(250);
#endif

  Serial.println("I2C bring-up via QMI8658_init()...");
  QMI8658_init();
  delay(50);

  Serial.println("TOUCH: FT3168_Init(FT3168_Gesture_Mode)...");
  FT3168_Init(FT3168_Gesture_Mode);
  delay(50);

  uint8_t id = (uint8_t)FT3168_ReadID();
  Serial.printf("TOUCH: FT3168_ReadID()=0x%02X (expected 0x03)\n", (unsigned)id);

  pinMode(IR_IN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_IN_PIN), IrIsr, FALLING);

#if !USE_LVGL_UI
  DrawSplash(id == 0x03 ? "Touch OK. Ready" : "Touch ID not 0x03");
  delay(300);

  Paint_NewImage((UBYTE*)gFrame, DISP_W, DISP_H, UI_ROTATION, WHITE);
  Paint_SetScale(65);
  Paint_SetRotate(UI_ROTATION);
#else
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  lv_time_attack_ui_init(HandleStartStop, HandleReset);
  lv_obj_invalidate(lv_scr_act());
  lv_timer_handler();
#endif

  gState = UI_IDLE;
  RenderState();
}

void loop() {
  static uint32_t lastPoll = 0;

#if USE_LVGL_UI
  static uint32_t lastTick = 0;
  uint32_t now = millis();
  uint32_t delta = now - lastTick;
  if (delta > 0) {
    lv_tick_inc(delta);
    lastTick = now;
  }
  lv_obj_invalidate(lv_scr_act());
  lv_timer_handler();
#else
  uint32_t now = millis();
#endif

  // Robust touch state
#if !USE_LVGL_UI
  static bool touchDown = false;
  static uint32_t lastTouchSampleMs = 0;

  // last known coords
  static uint16_t lastRawX = 0, lastRawY = 0;
  static uint16_t lastNormX = 0, lastNormY = 0;
  static uint16_t lastX = 0, lastY = 0;

#endif
  const bool pollReady = (uint32_t)(now - lastPoll) >= POLL_MS;
  if (pollReady) {
    lastPoll = now;
  }

  // Try to get a sample
#if !USE_LVGL_UI
  if (!pollReady) return;
  uint16_t rawX = 0, rawY = 0;
  bool gotSample = ReadTouchSample(rawX, rawY);

  if (gotSample) {
    lastTouchSampleMs = now;
    lastRawX = rawX;
    lastRawY = rawY;
    NormalizeTouch(rawX, rawY, lastNormX, lastNormY);
    RotateTouch(lastNormX, lastNormY, lastX, lastY);
  }

  bool downNow = (now - lastTouchSampleMs) <= TOUCH_HOLD_MS;
  bool justPressed = (!touchDown && downNow);
  bool justReleased = (touchDown && !downNow);
  touchDown = downNow;
#endif

  // IR events
  if (!pollReady) {
#if USE_LVGL_UI
    // Skip polling-sensitive logic but allow LVGL updates below.
#else
    return;
#endif
  }
  bool irTriggered = false;
  uint32_t irMs = 0;
  if (gIrSeen) {
    noInterrupts();
    irTriggered = gIrSeen;
    irMs = gIrSeenMs;
    gIrSeen = false;
    interrupts();
  }

  if (irTriggered && (irMs - gLastLapTriggerMs) >= LAP_LOCKOUT_MS) {
    gLastLapTriggerMs = irMs;
    if (gState == UI_ARMED) {
      Serial.println("IR: start run");
      StartRunning(irMs);
    } else if (gState == UI_RUNNING) {
      gLapCount++;
      gLastLapMs = irMs - gLastLapStartMs;
      gLastLapStartMs = irMs;
      gSessionMs = irMs - gStartMs;
      if (gBestLapMs == 0 || gLastLapMs < gBestLapMs) {
        gBestLapMs = gLastLapMs;
      }
      gDeltaMs = (int32_t)gLastLapMs - (int32_t)gBestLapMs;

      Serial.printf("LAP %u time=%lu ms\n", (unsigned)gLapCount, (unsigned long)gLastLapMs);

      if ((irMs - gLastBeepMs) >= BEEP_DEBOUNCE_MS) {
        BeepLap();
        gLastBeepMs = irMs;
      }

      if (gLapCount >= gSelectedLaps) {
        BeepComplete();
        FinishRun(irMs);
      } else {
        RenderState();
      }
    }
  }

  if (gState == UI_RUNNING) {
    gSessionMs = now - gStartMs;
#if !USE_LVGL_UI
    if ((uint32_t)(now - gLastUiMs) >= UI_REFRESH_MS) {
      gLastUiMs = now;
      RenderState();
    }
#endif
  }

#if USE_LVGL_UI
  if (gUiDirty || (uint32_t)(now - gLastLvglUiMs) >= LVGL_UI_REFRESH_MS) {
    gLastLvglUiMs = now;
    gUiDirty = false;
    UiSnapshot snapshot{};
    snapshot.state = gState;
    snapshot.selectedDriver = gSelectedDriver;
    snapshot.selectedLaps = gSelectedLaps;
    snapshot.lapCount = gLapCount;
    snapshot.sessionMs = gSessionMs;
    snapshot.lastLapMs = gLastLapMs;
    snapshot.bestLapMs = gBestLapMs;
    snapshot.deltaMs = gDeltaMs;
    if (gState == UI_RUNNING) {
      snapshot.currentLapMs = (gLapCount == 0) ? (now - gStartMs) : (now - gLastLapStartMs);
    } else if (gState == UI_FINISHED) {
      snapshot.currentLapMs = gLastLapMs;
    } else {
      snapshot.currentLapMs = 0;
    }
    lv_time_attack_ui_update(snapshot);
  }
#endif

#if !USE_LVGL_UI
  if (justPressed) {
#if TOUCH_DEBUG
    Serial.printf("TOUCH DOWN raw(%u,%u) norm(%u,%u) rot(%u,%u)\n",
                  (unsigned)lastRawX, (unsigned)lastRawY,
                  (unsigned)lastNormX, (unsigned)lastNormY,
                  (unsigned)lastX, (unsigned)lastY);
#else
    Serial.printf("TOUCH DOWN rot(%u,%u)\n", (unsigned)lastX, (unsigned)lastY);
#endif
  }
  if (justReleased) {
#if TOUCH_DEBUG
    Serial.println("TOUCH UP");
#endif
  }

  if (justPressed) {
    if (gState == UI_IDLE) {
      bool changed = false;
      if (HitTest(BTN_DRIVER_MINUS, lastX, lastY)) {
        gSelectedDriver = (gSelectedDriver == 1) ? MAX_DRIVERS : (gSelectedDriver - 1);
        changed = true;
      } else if (HitTest(BTN_DRIVER_PLUS, lastX, lastY)) {
        gSelectedDriver = (gSelectedDriver == MAX_DRIVERS) ? 1 : (gSelectedDriver + 1);
        changed = true;
      } else if (HitTest(BTN_LAP_MINUS, lastX, lastY)) {
        gSelectedLaps = (gSelectedLaps > MIN_LAPS) ? (gSelectedLaps - 1) : MIN_LAPS;
        changed = true;
      } else if (HitTest(BTN_LAP_PLUS, lastX, lastY)) {
        gSelectedLaps = (gSelectedLaps < MAX_LAPS) ? (gSelectedLaps + 1) : MAX_LAPS;
        changed = true;
      } else if (HitTest(BTN_START, lastX, lastY)) {
        StartArmed();
        if ((now - gLastBeepMs) >= BEEP_DEBOUNCE_MS) {
          BeepNow(2400, 35);
          gLastBeepMs = now;
        }
      }
      if (changed) {
        RenderState();
      }
    } else if (gState == UI_ARMED) {
      if (HitTest(BTN_CANCEL, lastX, lastY)) {
        gState = UI_IDLE;
        RenderState();
      }
    } else if (gState == UI_FINISHED) {
      if (HitTest(BTN_VIEW_STATS, lastX, lastY)) {
        gState = UI_STATS;
        RenderState();
      } else if (HitTest(BTN_DONE, lastX, lastY)) {
        gState = UI_IDLE;
        RenderState();
      }
    } else if (gState == UI_STATS) {
      if (HitTest(BTN_BACK, lastX, lastY)) {
        gState = UI_FINISHED;
        RenderState();
      }
    }
  }
#endif
}
