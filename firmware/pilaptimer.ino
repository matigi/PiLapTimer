#include <Arduino.h>

#include "waveshare_drivers/AMOLED_1in64.h"
#include "waveshare_drivers/DEV_Config.h"
#include "waveshare_drivers/GUI_Paint.h"
#include "waveshare_drivers/qspi_pio.h"

namespace {
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorWhite = 0xFFFF;
constexpr uint32_t kUptimeUpdateMs = 250;
constexpr uint32_t kSerialWaitMs = 2000;
constexpr int16_t kTitleX = 12;
constexpr int16_t kTitleY = 20;
constexpr int16_t kUptimeX = 12;
constexpr int16_t kUptimeY = 60;
constexpr int16_t kUptimeWidth = 240;
constexpr int16_t kUptimeHeight = 32;

uint32_t lastUptimeUpdateMs = 0;
uint16_t framebuffer[AMOLED_1IN64_WIDTH * AMOLED_1IN64_HEIGHT];

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
void initUsbSerial() {
  SerialUSB.begin(115200);
}

void printUsbSerial(const char *message) {
  if (SerialUSB) {
    SerialUSB.println(message);
  }
}
#else
void initUsbSerial() {}
void printUsbSerial(const char *message) {
  (void)message;
}
#endif

void initSerial() {
  Serial.begin(115200);
  initUsbSerial();
  delay(1500);

  const uint32_t start = millis();
  while (!Serial && millis() - start < kSerialWaitMs) {
    delay(10);
  }
  Serial.println("BOOT");
  printUsbSerial("BOOT");
}

void initDisplay() {
  DEV_Module_Init();
  QSPI_GPIO_Init();
  QSPI_PIO_Init();
  QSPI_1Wrie_Mode();
  AMOLED_1IN64_Init();
  AMOLED_1IN64_SetBrightness(100);
  Paint_NewImage(framebuffer, AMOLED_1IN64_WIDTH, AMOLED_1IN64_HEIGHT, 0, kColorWhite);
  Paint_Clear(kColorWhite);
  Paint_DrawString_EN(kTitleX, kTitleY, "PiLapTimer", kColorBlack, kColorWhite, 2);
}

void drawUptime(uint32_t nowMs) {
  const uint32_t seconds = nowMs / 1000;
  const uint32_t millisPart = nowMs % 1000;
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "Uptime: %lu.%03lu s", seconds, millisPart);

  Paint_ClearWindows(kUptimeX, kUptimeY, kUptimeX + kUptimeWidth, kUptimeY + kUptimeHeight, kColorWhite);
  Paint_DrawString_EN(kUptimeX, kUptimeY, buffer, kColorBlack, kColorWhite, 2);
}
}  // namespace

void setup() {
  initSerial();

  initDisplay();
  drawUptime(millis());
  lastUptimeUpdateMs = millis();
  AMOLED_1IN64_Display();
}

void loop() {
  const uint32_t nowMs = millis();
  if (nowMs - lastUptimeUpdateMs >= kUptimeUpdateMs) {
    drawUptime(nowMs);
    AMOLED_1IN64_Display();
    lastUptimeUpdateMs = nowMs;
  }
}
