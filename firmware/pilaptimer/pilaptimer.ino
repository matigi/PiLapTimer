#include <Arduino.h>

#include "DEV_Config.h"
#include "qspi_pio.h"
#include "AMOLED_1in64.h"
#include "GUI_Paint.h"

namespace {
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorWhite = 0xFFFF;
constexpr uint32_t kUptimeUpdateMs = 250;
constexpr int16_t kTitleX = 12;
constexpr int16_t kTitleY = 20;
constexpr int16_t kUptimeX = 12;
constexpr int16_t kUptimeY = 60;
constexpr int16_t kUptimeWidth = 220;
constexpr int16_t kUptimeHeight = 32;

uint32_t lastUptimeUpdateMs = 0;

void initDisplay() {
  DEV_Module_Init();
  QSPI_PIO_Init();
  AMOLED_1in64_Init();
  Paint_Init(AMOLED_1IN64_WIDTH, AMOLED_1IN64_HEIGHT);
  Paint_Clear(kColorBlack);
  Paint_DrawString_EN(kTitleX, kTitleY, "PiLapTimer", kColorWhite, kColorBlack, 2);
}

void drawUptime(uint32_t nowMs) {
  const uint32_t seconds = nowMs / 1000;
  const uint32_t millisPart = nowMs % 1000;
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "Uptime: %lu.%03lu s", seconds, millisPart);

  Paint_ClearWindows(kUptimeX, kUptimeY, kUptimeX + kUptimeWidth, kUptimeY + kUptimeHeight, kColorBlack);
  Paint_DrawString_EN(kUptimeX, kUptimeY, buffer, kColorWhite, kColorBlack, 2);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("BOOT");


  initDisplay();
  drawUptime(millis());
  lastUptimeUpdateMs = millis();
}

void loop() {
  const uint32_t nowMs = millis();
  if (nowMs - lastUptimeUpdateMs >= kUptimeUpdateMs) {
    drawUptime(nowMs);
    lastUptimeUpdateMs = nowMs;
  }
}
