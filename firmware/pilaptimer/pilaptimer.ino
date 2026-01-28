#include <Arduino.h>

#include "DEV_Config.h"
#include "AMOLED_1in64.h"
#include "qspi_pio.h"
#include "QMI8658.h"
#include "FT3168.h"
#include "GUI_Paint.h"
#include "fonts.h"

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

static UWORD* gFrame = nullptr;

// ----------------- UI helpers -----------------
struct Button {
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
  const char* label;
};

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

static void DrawButton(const Button &btn, uint16_t textColor = WHITE, uint16_t fillColor = BLUE) {
  Paint_DrawRectangle(btn.x, btn.y, btn.x + btn.w, btn.y + btn.h, fillColor, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawRectangle(btn.x, btn.y, btn.x + btn.w, btn.y + btn.h, textColor, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  uint16_t textX = btn.x + 6;
  uint16_t textY = btn.y + 8;
  Paint_DrawString_EN(textX, textY, btn.label, &Font16, fillColor, textColor);
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
enum UiState {
  UI_BOOT,
  UI_IDLE,
  UI_ARMED,
  UI_RUNNING,
  UI_FINISHED,
  UI_STATS
};

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

// Buttons (idle)
static const Button BTN_DRIVER_MINUS = {18, 70, 40, 34, "-"};
static const Button BTN_DRIVER_PLUS  = {220, 70, 40, 34, "+"};
static const Button BTN_LAP_MINUS    = {18, 130, 40, 34, "-"};
static const Button BTN_LAP_PLUS     = {220, 130, 40, 34, "+"};
static const Button BTN_START        = {40, 190, 200, 44, "START RUN"};

// Buttons (armed)
static const Button BTN_CANCEL       = {70, 300, 140, 40, "CANCEL"};

// Buttons (finished)
static const Button BTN_VIEW_STATS   = {20, 320, 110, 40, "STATS"};
static const Button BTN_DONE         = {150, 320, 110, 40, "DONE"};

// Buttons (stats)
static const Button BTN_BACK         = {80, 360, 120, 40, "BACK"};

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

  Paint_DrawString_EN(10, 10, "Time Attack", &Font20, BLACK, WHITE);

  char line[48];
  snprintf(line, sizeof(line), "Driver: %u", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(80, 74, line, &Font16, BLACK, WHITE);

  snprintf(line, sizeof(line), "Laps: %u", (unsigned)gSelectedLaps);
  Paint_DrawString_EN(100, 134, line, &Font16, BLACK, WHITE);

  DrawButton(BTN_DRIVER_MINUS);
  DrawButton(BTN_DRIVER_PLUS);
  DrawButton(BTN_LAP_MINUS);
  DrawButton(BTN_LAP_PLUS);

  DrawButton(BTN_START, WHITE, GREEN);

  RunStats &run = gDriverRuns[gSelectedDriver - 1];
  Paint_DrawString_EN(10, 250, "Last Run", &Font16, BLACK, YELLOW);

  char timeBuf[24];
  if (run.valid) {
    FormatTime(timeBuf, sizeof(timeBuf), run.totalMs);
    snprintf(line, sizeof(line), "Total: %s", timeBuf);
    Paint_DrawString_EN(10, 275, line, &Font16, BLACK, WHITE);

    FormatTime(timeBuf, sizeof(timeBuf), run.bestMs);
    snprintf(line, sizeof(line), "Best:  %s", timeBuf);
    Paint_DrawString_EN(10, 295, line, &Font16, BLACK, WHITE);
  } else {
    Paint_DrawString_EN(10, 275, "No run recorded", &Font16, BLACK, WHITE);
  }

  AMOLED_1IN64_Display(gFrame);
}

static void DrawArmedScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[48];
  snprintf(line, sizeof(line), "Driver: %u", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(10, 20, line, &Font20, BLACK, WHITE);

  snprintf(line, sizeof(line), "Target Laps: %u", (unsigned)gSelectedLaps);
  Paint_DrawString_EN(10, 50, line, &Font16, BLACK, WHITE);

  Paint_DrawString_EN(10, 120, "READY", &Font24, BLACK, GREEN);
  Paint_DrawString_EN(10, 160, "Cross start line", &Font16, BLACK, WHITE);

  DrawButton(BTN_CANCEL, WHITE, RED);

  AMOLED_1IN64_Display(gFrame);
}

static void DrawRunningScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[64];
  snprintf(line, sizeof(line), "Driver: %u", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(10, 10, line, &Font16, BLACK, WHITE);

  uint8_t lapDisplay = (gLapCount < gSelectedLaps) ? (gLapCount + 1) : gSelectedLaps;
  snprintf(line, sizeof(line), "Lap: %u / %u", (unsigned)lapDisplay, (unsigned)gSelectedLaps);
  Paint_DrawString_EN(10, 40, line, &Font20, BLACK, WHITE);

  char timeBuf[24];
  FormatTimeMaybe(timeBuf, sizeof(timeBuf), gLapCount > 0, gLastLapMs);
  snprintf(line, sizeof(line), "Last: %s", timeBuf);
  Paint_DrawString_EN(10, 90, line, &Font16, BLACK, WHITE);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), gBestLapMs > 0, gBestLapMs);
  snprintf(line, sizeof(line), "Best: %s", timeBuf);
  Paint_DrawString_EN(10, 115, line, &Font16, BLACK, WHITE);

  if (gLapCount > 0 && gBestLapMs > 0) {
    long delta = (long)gDeltaMs;
    snprintf(line, sizeof(line), "Delta: %c%lu ms", (delta >= 0) ? '+' : '-', (unsigned long)labs(delta));
    Paint_DrawString_EN(10, 140, line, &Font16, BLACK, (delta <= 0) ? GREEN : RED);
  }

  FormatTime(timeBuf, sizeof(timeBuf), gSessionMs);
  snprintf(line, sizeof(line), "Session: %s", timeBuf);
  Paint_DrawString_EN(10, 180, line, &Font20, BLACK, WHITE);

  Paint_DrawString_EN(10, 220, "IR OK", &Font16, BLACK, GREEN);

  AMOLED_1IN64_Display(gFrame);
}

static void DrawFinishedScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[64];
  snprintf(line, sizeof(line), "Driver: %u", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(10, 10, line, &Font16, BLACK, WHITE);

  snprintf(line, sizeof(line), "Laps: %u", (unsigned)gSelectedLaps);
  Paint_DrawString_EN(10, 32, line, &Font16, BLACK, WHITE);

  RunStats &run = gDriverRuns[gSelectedDriver - 1];
  char timeBuf[24];

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), run.valid, run.totalMs);
  snprintf(line, sizeof(line), "Total: %s", timeBuf);
  Paint_DrawString_EN(10, 70, line, &Font20, BLACK, WHITE);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), run.valid, run.bestMs);
  snprintf(line, sizeof(line), "Best:  %s", timeBuf);
  Paint_DrawString_EN(10, 105, line, &Font16, BLACK, WHITE);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), run.valid, run.avgMs);
  snprintf(line, sizeof(line), "Avg:   %s", timeBuf);
  Paint_DrawString_EN(10, 130, line, &Font16, BLACK, WHITE);

  DrawButton(BTN_VIEW_STATS, WHITE, BLUE);
  DrawButton(BTN_DONE, WHITE, GREEN);

  AMOLED_1IN64_Display(gFrame);
}

static void DrawStatsScreen() {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  char line[64];
  RunStats &selected = gDriverRuns[gSelectedDriver - 1];
  snprintf(line, sizeof(line), "Driver %u - Last Run", (unsigned)gSelectedDriver);
  Paint_DrawString_EN(10, 10, line, &Font16, BLACK, WHITE);

  char timeBuf[24];
  FormatTimeMaybe(timeBuf, sizeof(timeBuf), selected.valid, selected.totalMs);
  snprintf(line, sizeof(line), "Total: %s", timeBuf);
  Paint_DrawString_EN(10, 40, line, &Font16, BLACK, WHITE);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), selected.valid, selected.bestMs);
  snprintf(line, sizeof(line), "Best:  %s", timeBuf);
  Paint_DrawString_EN(10, 60, line, &Font16, BLACK, WHITE);

  FormatTimeMaybe(timeBuf, sizeof(timeBuf), selected.valid, selected.avgMs);
  snprintf(line, sizeof(line), "Avg:   %s", timeBuf);
  Paint_DrawString_EN(10, 80, line, &Font16, BLACK, WHITE);

  Paint_DrawString_EN(10, 130, "Previous Driver", &Font16, BLACK, YELLOW);

  int compareDriver = gLastCompletedDriver;
  if (compareDriver == (int)gSelectedDriver) {
    compareDriver = -1;
  }

  if (compareDriver > 0) {
    RunStats &other = gDriverRuns[compareDriver - 1];
    snprintf(line, sizeof(line), "Driver %u", (unsigned)compareDriver);
    Paint_DrawString_EN(10, 150, line, &Font16, BLACK, WHITE);
    FormatTimeMaybe(timeBuf, sizeof(timeBuf), other.valid, other.bestMs);
    snprintf(line, sizeof(line), "Best: %s", timeBuf);
    Paint_DrawString_EN(10, 170, line, &Font16, BLACK, WHITE);
  } else {
    Paint_DrawString_EN(10, 150, "No previous run", &Font16, BLACK, WHITE);
  }

  DrawButton(BTN_BACK, WHITE, BLUE);

  AMOLED_1IN64_Display(gFrame);
}

static void RenderState() {
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

  UDOUBLE bytes = (UDOUBLE)DISP_W * (UDOUBLE)DISP_H * 2;
  gFrame = (UWORD*)malloc(bytes);
  if (!gFrame) {
    Serial.println("FATAL: framebuffer malloc failed");
    while (true) delay(1000);
  }

  Paint_NewImage((UBYTE*)gFrame, DISP_W, DISP_H, 0, WHITE);
  Paint_SetScale(65);
  Paint_SetRotate(ROTATE_0);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.printf("BUZZER: test on pin %u\n", (unsigned)BUZZER_PIN);
  BeepNow();
  delay(200);
  BeepNow();

  DrawSplash("Booting...");
  delay(250);

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

  DrawSplash(id == 0x03 ? "Touch OK. Ready" : "Touch ID not 0x03");
  delay(300);

  gState = UI_IDLE;
  RenderState();
}

void loop() {
  static uint32_t lastPoll = 0;

  // Robust touch state
  static bool touchDown = false;
  static uint32_t lastTouchSampleMs = 0;

  // last known coords
  static uint16_t lastRawX = 0, lastRawY = 0;
  static uint16_t lastX = 0, lastY = 0;

  // beep debounce
  static uint32_t lastBeepMs = 0;

  uint32_t now = millis();
  if ((uint32_t)(now - lastPoll) < POLL_MS) return;
  lastPoll = now;

  // Try to get a sample
  uint16_t rawX = 0, rawY = 0;
  bool gotSample = ReadTouchSample(rawX, rawY);

  if (gotSample) {
    lastTouchSampleMs = now;
    lastRawX = rawX;
    lastRawY = rawY;
    NormalizeTouch(rawX, rawY, lastX, lastY);
  }

  bool downNow = (now - lastTouchSampleMs) <= TOUCH_HOLD_MS;
  bool justPressed = (!touchDown && downNow);
  bool justReleased = (touchDown && !downNow);
  touchDown = downNow;

  // IR events
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

      if ((irMs - lastBeepMs) >= BEEP_DEBOUNCE_MS) {
        BeepLap();
        lastBeepMs = irMs;
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
    if ((uint32_t)(now - gLastUiMs) >= UI_REFRESH_MS) {
      gLastUiMs = now;
      RenderState();
    }
  }

  if (justPressed) {
    Serial.printf("TOUCH DOWN raw(%u,%u) norm(%u,%u)\n",
                  (unsigned)lastRawX, (unsigned)lastRawY, (unsigned)lastX, (unsigned)lastY);
  }
  if (justReleased) {
    Serial.println("TOUCH UP");
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
        if ((now - lastBeepMs) >= BEEP_DEBOUNCE_MS) {
          BeepNow(2400, 35);
          lastBeepMs = now;
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
}
