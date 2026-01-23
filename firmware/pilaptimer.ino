#include <Arduino.h>

// Display configuration: prefer Waveshare AMOLED library, fall back to Adafruit_GFX-compatible driver.
#if __has_include(<Waveshare_AMOLED.h>)
#include <Waveshare_AMOLED.h>
static Waveshare_AMOLED display;
#elif __has_include(<AMOLED_1in64.h>)
#include <AMOLED_1in64.h>
static AMOLED_1in64 display;
#elif __has_include(<LCD_1inch64.h>)
#include <LCD_1inch64.h>
static LCD_1inch64 display;
#elif __has_include(<Adafruit_GFX.h>) && __has_include(<Adafruit_ST7789.h>)
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#ifndef PILAPTIMER_TFT_CS
#define PILAPTIMER_TFT_CS 17
#endif
#ifndef PILAPTIMER_TFT_DC
#define PILAPTIMER_TFT_DC 16
#endif
#ifndef PILAPTIMER_TFT_RST
#define PILAPTIMER_TFT_RST 15
#endif

static Adafruit_ST7789 display(PILAPTIMER_TFT_CS, PILAPTIMER_TFT_DC, PILAPTIMER_TFT_RST);
#else
#error "No supported display library found. Install the Waveshare AMOLED library or an Adafruit_GFX-compatible driver."
#endif

namespace {
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorWhite = 0xFFFF;
constexpr uint32_t kUptimeUpdateMs = 250;
constexpr int16_t kTitleX = 12;
constexpr int16_t kTitleY = 20;
constexpr int16_t kUptimeX = 12;
constexpr int16_t kUptimeY = 60;

uint32_t lastUptimeUpdateMs = 0;

void initDisplay() {
#if defined(ADAFRUIT_ST7789_H)
  display.init(280, 456);
#else
  display.begin();
#endif
  display.fillScreen(kColorBlack);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setTextColor(kColorWhite);
  display.setCursor(kTitleX, kTitleY);
  display.print("PiLapTimer");
}

void drawUptime(uint32_t nowMs) {
  const uint32_t seconds = nowMs / 1000;
  const uint32_t millisPart = nowMs % 1000;
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "Uptime: %lu.%03lu s", seconds, millisPart);

  display.setTextColor(kColorWhite, kColorBlack);
  display.setCursor(kUptimeX, kUptimeY);
  display.print(buffer);
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
