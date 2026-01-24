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

#ifndef RED
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F
#define WHITE 0xFFFF
#define BLACK 0x0000
#endif

// ==============================
// IR framed receiver (tuned)
// ==============================
namespace ir {

static const uint8_t IR_IN_PIN = 2;

// Beacon: 10x(2ms ON / 2ms OFF) then ~20ms gap
static const uint32_t FRAME_GAP_MIN_US = 19000;

// Demod output tolerances
static const uint32_t ON_MIN_US  = 900;
static const uint32_t ON_MAX_US  = 4000;
static const uint32_t OFF_MIN_US = 900;
static const uint32_t OFF_MAX_US = 5000;

static const uint8_t  MIN_VALID_BURSTS_FOR_FRAME = 10;
static const uint32_t BURST_WINDOW_MS = 120;
static const uint32_t COOLDOWN_MS = 2000;

static inline bool inRange(uint32_t v, uint32_t lo, uint32_t hi) {
  return (v >= lo && v <= hi);
}

volatile uint32_t lastEdgeUs = 0;
volatile bool lastLevel = true;
volatile uint32_t lastOnUs = 0;

volatile uint16_t validBurstCount = 0;
volatile uint16_t framesSeen = 0;
volatile uint16_t noiseEdges = 0;

volatile uint32_t burstWindowStartMs = 0;
volatile uint32_t pendingFrameMs = 0;

void IRAM_ATTR onIrEdge() {
  uint32_t nowUs = (uint32_t)micros();
  bool level = digitalRead(IR_IN_PIN);

  if (lastEdgeUs == 0) {
    lastEdgeUs = nowUs;
    lastLevel = level;
    return;
  }

  uint32_t durUs = nowUs - lastEdgeUs;

  if (lastLevel == false) {
    // ON (LOW)
    if (inRange(durUs, ON_MIN_US, ON_MAX_US)) {
      lastOnUs = durUs;
    } else {
      lastOnUs = 0;
      noiseEdges++;
    }
  } else {
    // OFF (HIGH)
    uint32_t nowMs = (uint32_t)millis();

    if (burstWindowStartMs != 0 && (nowMs - burstWindowStartMs) > BURST_WINDOW_MS) {
      validBurstCount = 0;
      burstWindowStartMs = 0;
      lastOnUs = 0;
    }

    if (durUs >= FRAME_GAP_MIN_US) {
      if (validBurstCount >= MIN_VALID_BURSTS_FOR_FRAME) {
        framesSeen++;
        pendingFrameMs = nowMs;
      }
      validBurstCount = 0;
      burstWindowStartMs = 0;
      lastOnUs = 0;

    } else if (inRange(durUs, OFF_MIN_US, OFF_MAX_US)) {
      if (lastOnUs != 0) {
        if (validBurstCount == 0) burstWindowStartMs = nowMs;
        validBurstCount++;
        lastOnUs = 0;
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

void Init() {
  pinMode(IR_IN_PIN, INPUT_PULLUP);
  lastEdgeUs = 0;
  lastLevel = digitalRead(IR_IN_PIN);
  attachInterrupt(digitalPinToInterrupt(IR_IN_PIN), onIrEdge, CHANGE);
}

bool ConsumeLapEvent(uint32_t* out_ms) {
  static uint32_t lastLapMs = 0;

  uint32_t frameMs = 0;
  noInterrupts();
  frameMs = pendingFrameMs;
  pendingFrameMs = 0;
  interrupts();

  if (frameMs == 0) return false;
  if ((frameMs - lastLapMs) < COOLDOWN_MS) return false;

  lastLapMs = frameMs;
  if (out_ms) *out_ms = frameMs;
  return true;
}

void PrintStatsIfDue() {
  static uint32_t lastPrintMs = 0;
  uint32_t now = millis();
  if (now - lastPrintMs < 500) return;
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

  Serial.printf("IR t=%lu | lvl=%s | bursts=%u | frames=%u | noise=%u | winAge=%lu\n",
                (unsigned long)now,
                lvl ? "HIGH" : "LOW",
                (unsigned)bursts,
                (unsigned)f,
                (unsigned)n,
                (winStart == 0) ? 0UL : (unsigned long)(now - winStart));
}

} // namespace ir

// ==============================
// UI / Display / Touch
// ==============================
namespace {

UWORD* gFrame = nullptr;

bool last_touch_active = false;
uint16_t last_touch_x = 0;
uint16_t last_touch_y = 0;

// Lap tracking
constexpr uint32_t kMinLapTimeMs = 5000;
uint32_t lap_count = 0;
uint32_t last_valid_lap_ms = 0;

void DrawUI(bool touch_active, uint16_t x, uint16_t y) {
  if (!gFrame) return;

  Paint_SelectImage((UBYTE*)gFrame);
  Paint_Clear(BLACK);

  Paint_DrawString_EN(8, 6, "PiLapTimer", &Font20, WHITE, BLACK);

  char lap_text[32];
  snprintf(lap_text, sizeof(lap_text), "Lap: %lu", (unsigned long)lap_count);
  Paint_DrawString_EN(8, 32, lap_text, &Font20, GREEN, BLACK);

  if (touch_active) {
    char xy[32];
    snprintf(xy, sizeof(xy), "Touch %u,%u", x, y);
    Paint_DrawString_EN(8, 60, xy, &Font16, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(8, 60, "No touch", &Font16, RED, BLACK);
  }

  AMOLED_1IN64_Display(gFrame);
}

} // namespace

void setup() {
  Serial.begin(115200);

  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { delay(10); }

  Serial.println();
  Serial.println("BOOT: starting PiLapTimer (RP2350)");
  Serial.println("BOOT: Serial OK");

  // IR early
  ir::Init();
  Serial.println("BOOT: IR init OK (GPIO2)");

  // Match Waveshare demo init sequence
  Serial.println("BOOT: DEV_Module_Init...");
  if (DEV_Module_Init() != 0) {
    Serial.println("BOOT: DEV_Module_Init FAILED");
    while (true) delay(1000);
  }
  Serial.println("BOOT: DEV_Module_Init OK");

  Serial.println("BOOT: QSPI init...");
  // These calls are REQUIRED before AMOLED_1IN64_Init on this board (per demo)
  QSPI_GPIO_Init(qspi);
  QSPI_PIO_Init(qspi);
  QSPI_1Wrie_Mode(&qspi);
  Serial.println("BOOT: QSPI init OK");

  Serial.println("BOOT: AMOLED_1IN64_Init...");
  AMOLED_1IN64_Init();
  Serial.println("BOOT: AMOLED init OK");

  AMOLED_1IN64_SetBrightness(100);
  AMOLED_1IN64_Clear(WHITE);

  // Allocate full framebuffer like demo
  UDOUBLE imageSizeBytes = (UDOUBLE)AMOLED_1IN64_HEIGHT * (UDOUBLE)AMOLED_1IN64_WIDTH * 2;
  gFrame = (UWORD*)malloc(imageSizeBytes);
  if (!gFrame) {
    Serial.println("BOOT: framebuffer malloc FAILED");
    while (true) delay(1000);
  }
  Serial.println("BOOT: framebuffer malloc OK");

  Paint_NewImage((UBYTE*)gFrame, AMOLED_1IN64.WIDTH, AMOLED_1IN64.HEIGHT, 0, WHITE);
  Paint_SetScale(65);
  Paint_SetRotate(ROTATE_0);
  Paint_Clear(WHITE);
  AMOLED_1IN64_Display(gFrame);

  Serial.println("BOOT: Touch init...");
  // Use Point mode for normal XY reporting (matches common demo use)
  FT3168_Init(FT3168_Point_Mode);
  Serial.println("BOOT: Touch init OK");

  Serial.println("BOOT: init complete");
  DrawUI(false, 0, 0);
}

void loop() {
  // IR lap event
  uint32_t t_ms = 0;
  if (ir::ConsumeLapEvent(&t_ms)) {
    if (last_valid_lap_ms != 0) {
      uint32_t lap_delta = t_ms - last_valid_lap_ms;
      if (lap_delta >= kMinLapTimeMs) {
        lap_count++;
        last_valid_lap_ms = t_ms;
        Serial.printf("LAP %lu @ %lu ms | delta=%lu ms\n",
                      (unsigned long)lap_count,
                      (unsigned long)t_ms,
                      (unsigned long)lap_delta);
      } else {
        Serial.printf("IGNORE (min lap) @ %lu ms | delta=%lu ms\n",
                      (unsigned long)t_ms,
                      (unsigned long)lap_delta);
      }
    } else {
      lap_count++;
      last_valid_lap_ms = t_ms;
      Serial.printf("LAP %lu @ %lu ms | delta=0\n",
                    (unsigned long)lap_count,
                    (unsigned long)t_ms);
    }
    // Update UI immediately on lap
    DrawUI(last_touch_active, last_touch_x, last_touch_y);
  }

  // Touch
  bool touch_active = FT3168_Get_Point();
  uint16_t touch_x = FT3168.x_point;
  uint16_t touch_y = FT3168.y_point;

  if (touch_active != last_touch_active ||
      (touch_active && (touch_x != last_touch_x || touch_y != last_touch_y))) {
    last_touch_active = touch_active;
    last_touch_x = touch_x;
    last_touch_y = touch_y;
    DrawUI(touch_active, touch_x, touch_y);
  }

  ir::PrintStatsIfDue();
}
