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
#endif

static const uint16_t DISP_W = 280;
static const uint16_t DISP_H = 456;

// IMPORTANT: Do NOT use GP6/GP7 (I2C). Use GP16 (wire buzzer + to GP16, - to GND).
static const uint8_t  BUZZER_PIN = 16;

// Polling + state windowing
static const uint16_t POLL_MS              = 10;
static const uint16_t TOUCH_HOLD_MS        = 120;  // touch considered "down" for this long after last valid sample
static const uint16_t BEEP_DEBOUNCE_MS     = 250;  // minimum time between beeps

static UWORD* gFrame = nullptr;

// ----------------- Beep (no tone) -----------------
static void BeepNow() {
  // PWM beep without Arduino tone() (more compatible on RP2040/RP2350)
  const uint16_t freq = 2600;
  const uint16_t ms   = 25;

  analogWriteFreq(freq);
  analogWrite(BUZZER_PIN, 128);   // 50% duty (0..255)
  delay(ms);
  analogWrite(BUZZER_PIN, 0);
}

// ----------------- Touch helpers -----------------
static inline bool TouchLooksInvalid(uint16_t x, uint16_t y) {
  if (x == 4095 && y == 4095) return true;
  if (x == 0xFFFF && y == 0xFFFF) return true;
  return false;
}

// Reads ONE sample if available. This may only return true once per press on your current stack.
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

  // scale if it looks like 0..4095 range
  if (rawX > (DISP_W + 50) || rawY > (DISP_H + 50)) {
    x = (uint32_t)rawX * (DISP_W - 1) / 4095UL;
    y = (uint32_t)rawY * (DISP_H - 1) / 4095UL;
  }

  if (x >= DISP_W) x = DISP_W - 1;
  if (y >= DISP_H) y = DISP_H - 1;

  nx = (uint16_t)x;
  ny = (uint16_t)y;
}

// ----------------- UI -----------------
static void DrawSplash(const char* line2) {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);
  Paint_DrawString_EN(18, 150, "PiLapTimer", &Font24, BLACK, WHITE);
  Paint_DrawString_EN(18, 190, line2, &Font16, BLACK, GREEN);
  AMOLED_1IN64_Display(gFrame);
}

static void DrawTouchScreen(bool down, uint16_t x, uint16_t y, uint16_t rawX, uint16_t rawY, uint8_t id) {
  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  Paint_DrawString_EN(10, 10, "Touch Test (robust)", &Font20, BLACK, WHITE);

  char idLine[48];
  snprintf(idLine, sizeof(idLine), "FT3168 ID: 0x%02X", (unsigned)id);
  Paint_DrawString_EN(10, 40, idLine, &Font16, BLACK, (id == 0x03) ? GREEN : RED);

  if (!down) {
    Paint_DrawString_EN(10, 80, "Touch: (none)", &Font16, BLACK, WHITE);
    Paint_DrawString_EN(10, 110, "Tap screen to beep", &Font16, BLACK, GREEN);
  } else {
    Paint_DrawString_EN(10, 80, "Touch: DOWN", &Font16, BLACK, GREEN);

    char buf[64];
    snprintf(buf, sizeof(buf), "raw(%u,%u)", (unsigned)rawX, (unsigned)rawY);
    Paint_DrawString_EN(10, 110, buf, &Font16, BLACK, WHITE);
    snprintf(buf, sizeof(buf), "norm(%u,%u)", (unsigned)x, (unsigned)y);
    Paint_DrawString_EN(10, 130, buf, &Font16, BLACK, WHITE);

    // Crosshair
    int x1 = (x > 12) ? (x - 12) : 0;
    int x2 = (x + 12 < DISP_W) ? (x + 12) : (DISP_W - 1);
    int y1 = (y > 12) ? (y - 12) : 0;
    int y2 = (y + 12 < DISP_H) ? (y + 12) : (DISP_H - 1);

    Paint_DrawLine(x1, y, x2, y, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(x, y1, x, y2, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
  }

  AMOLED_1IN64_Display(gFrame);
}

// ----------------- Arduino -----------------
void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println();
  Serial.println("BOOT: PiLapTimer touch test (robust down/up + beep)");

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

  DrawSplash(id == 0x03 ? "Touch OK. Tap screen!" : "Touch ID not 0x03");
  delay(300);

  DrawTouchScreen(false, 0, 0, 0, 0, id);
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

  // Determine current down based on "recent sample"
  bool downNow = (now - lastTouchSampleMs) <= TOUCH_HOLD_MS;

  // Edge detection based on robust downNow
  bool justPressed = (!touchDown && downNow);
  bool justReleased = (touchDown && !downNow);

  if (justPressed) {
    Serial.printf("TOUCH DOWN raw(%u,%u) norm(%u,%u)\n",
                  (unsigned)lastRawX, (unsigned)lastRawY, (unsigned)lastX, (unsigned)lastY);

    // Beep on press (debounced)
    if ((now - lastBeepMs) >= BEEP_DEBOUNCE_MS) {
      Serial.println("BEEP: touch down");
      BeepNow();
      lastBeepMs = now;
    }

    DrawTouchScreen(true, lastX, lastY, lastRawX, lastRawY, (uint8_t)FT3168_ReadID());
  }

  if (justReleased) {
    Serial.println("TOUCH UP");
    DrawTouchScreen(false, 0, 0, 0, 0, (uint8_t)FT3168_ReadID());
  }

  touchDown = downNow;
}
