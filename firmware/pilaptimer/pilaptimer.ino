#include <Arduino.h>

#include "DEV_Config.h"
#include "AMOLED_1in64.h"
#include "FT3168.h"
#include "GUI_Paint.h"
#include "fonts.h"
#include "qspi_pio.h"

// Make IRAM_ATTR portable (ESP32 uses it; RP/Pico cores don't)
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// RGB565 colors
#ifndef RED
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F
#define WHITE 0xFFFF
#define BLACK 0x0000
#endif

namespace {
constexpr uint16_t kInfoHeight = 80;
static UWORD info_image[AMOLED_1IN64_WIDTH * kInfoHeight];

constexpr uint8_t kIrPin = 2;
constexpr bool kIrActiveLow = true;
constexpr uint32_t kLapDebounceMs = 1500;
constexpr uint32_t kMinLapTimeMs = 5000;

static const uint32_t kFrameGapMinUs = 16000;
static const uint32_t kOnMinUs = 900;
static const uint32_t kOnMaxUs = 4000;
static const uint32_t kOffMinUs = 900;
static const uint32_t kOffMaxUs = 6000;
static const uint8_t kMinValidBurstsForFrame = 8;

volatile uint32_t last_edge_us = 0;
volatile bool last_level = true;
volatile uint16_t valid_burst_count = 0;
volatile uint16_t frames_seen = 0;
volatile uint16_t noise_edges = 0;
volatile uint32_t last_on_us = 0;

volatile bool lap_triggered = false;
volatile uint32_t lap_triggered_ms = 0;

uint32_t lap_count = 0;
uint32_t last_valid_lap_ms = 0;
uint32_t last_trigger_ms = 0;
uint32_t last_lap_delta_ms = 0;
}  // namespace

static inline bool inRange(uint32_t v, uint32_t lo, uint32_t hi) {
  return (v >= lo && v <= hi);
}

void IRAM_ATTR OnIrEdge() {
  uint32_t now_us = micros();
  bool level = digitalRead(kIrPin);

  if (last_edge_us == 0) {
    last_edge_us = now_us;
    last_level = level;
    return;
  }

  uint32_t dur_us = now_us - last_edge_us;
  bool is_burst_level = kIrActiveLow ? (last_level == false)
                                     : (last_level == true);

  if (is_burst_level) {
    if (inRange(dur_us, kOnMinUs, kOnMaxUs)) {
      last_on_us = dur_us;
    } else {
      last_on_us = 0;
      noise_edges++;
    }
  } else {
    if (dur_us >= kFrameGapMinUs) {
      if (valid_burst_count >= kMinValidBurstsForFrame) {
        frames_seen++;
        lap_triggered_ms = millis();
        lap_triggered = true;
      }

      valid_burst_count = 0;
      last_on_us = 0;
    } else if (inRange(dur_us, kOffMinUs, kOffMaxUs)) {
      if (last_on_us != 0) {
        valid_burst_count++;
        last_on_us = 0;
      } else {
        noise_edges++;
      }
    } else {
      noise_edges++;
      last_on_us = 0;
    }
  }

  last_edge_us = now_us;
  last_level = level;
}

void RenderLapInfo(uint32_t laps, uint32_t last_lap_ms, uint32_t delta_ms) {
  Paint_SelectImage(reinterpret_cast<UBYTE *>(info_image));
  Paint_Clear(BLACK);
  Paint_DrawString_EN(8, 6, "Lap Timing", &Font20, WHITE, BLACK);

  char lap_text[24];
  char time_text[24];
  char delta_text[24];
  snprintf(lap_text, sizeof(lap_text), "Lap: %lu",
           static_cast<unsigned long>(laps));
  snprintf(time_text, sizeof(time_text), "Last: %lu ms",
           static_cast<unsigned long>(last_lap_ms));
  snprintf(delta_text, sizeof(delta_text), "Delta: %lu ms",
           static_cast<unsigned long>(delta_ms));

  Paint_DrawString_EN(8, 30, lap_text, &Font16, GREEN, BLACK);
  Paint_DrawString_EN(8, 46, time_text, &Font12, WHITE, BLACK);
  Paint_DrawString_EN(8, 62, delta_text, &Font12, WHITE, BLACK);

  AMOLED_1IN64_DisplayWindows(0, 0, AMOLED_1IN64_WIDTH, kInfoHeight,
                             info_image);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("BOOT");
  Serial.println("Starting display init...");

  int rc = DEV_Module_Init();
  Serial.print("DEV_Module_Init rc=");
  Serial.println(rc);

  Serial.println("QSPI_GPIO_Init");
  QSPI_GPIO_Init(qspi);

  Serial.println("QSPI_PIO_Init");
  QSPI_PIO_Init(qspi);

  Serial.println("QSPI_1Wrie_Mode");
  QSPI_1Wrie_Mode(&qspi);

  Serial.println("AMOLED_1IN64_Init");
  AMOLED_1IN64_Init();

  Serial.println("SetBrightness");
  AMOLED_1IN64_SetBrightness(100);

  Serial.println("Color clears");
  AMOLED_1IN64_Clear(RED);   DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(GREEN); DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(BLUE);  DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(WHITE); DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(BLACK); DEV_Delay_ms(300);

  Paint_NewImage(reinterpret_cast<UBYTE *>(info_image),
                 AMOLED_1IN64_WIDTH, kInfoHeight, ROTATE_0, BLACK);
  Paint_SetScale(65);
  Paint_SetMirroring(MIRROR_NONE);
  RenderLapInfo(0, 0, 0);

  pinMode(kIrPin, kIrActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  last_edge_us = 0;
  last_level = digitalRead(kIrPin);
  attachInterrupt(digitalPinToInterrupt(kIrPin), OnIrEdge,
                  kIrActiveLow ? FALLING : RISING);
  Serial.print("IR debounce ms=");
  Serial.println(kLapDebounceMs);
  Serial.print("Min lap ms=");
  Serial.println(kMinLapTimeMs);

  Serial.println("DONE");
}

void loop() {
  if (lap_triggered) {
    uint32_t timestamp_ms = 0;
    noInterrupts();
    if (lap_triggered) {
      timestamp_ms = lap_triggered_ms;
      lap_triggered = false;
    }
    interrupts();

    if (timestamp_ms != 0) {
      if (last_trigger_ms != 0) {
        uint32_t trigger_delta_ms = timestamp_ms - last_trigger_ms;
        if (trigger_delta_ms < kLapDebounceMs) {
          Serial.printf("IR IGNORE (debounce) @ %lu ms | delta=%lu ms\n",
                        static_cast<unsigned long>(timestamp_ms),
                        static_cast<unsigned long>(trigger_delta_ms));
          return;
        }
      }

      last_trigger_ms = timestamp_ms;

      if (last_valid_lap_ms != 0) {
        uint32_t lap_delta_ms = timestamp_ms - last_valid_lap_ms;
        if (lap_delta_ms < kMinLapTimeMs) {
          Serial.printf("IR IGNORE (min lap) @ %lu ms | delta=%lu ms\n",
                        static_cast<unsigned long>(timestamp_ms),
                        static_cast<unsigned long>(lap_delta_ms));
        } else {
          lap_count++;
          last_valid_lap_ms = timestamp_ms;
          last_lap_delta_ms = lap_delta_ms;
          Serial.printf("IR HIT @ %lu ms | lap=%lu | delta=%lu ms\n",
                        static_cast<unsigned long>(timestamp_ms),
                        static_cast<unsigned long>(lap_count),
                        static_cast<unsigned long>(lap_delta_ms));
          RenderLapInfo(lap_count, last_valid_lap_ms, last_lap_delta_ms);
        }
      } else {
        lap_count++;
        last_valid_lap_ms = timestamp_ms;
        last_lap_delta_ms = 0;
        Serial.printf("IR HIT @ %lu ms | lap=%lu | delta=0 ms\n",
                      static_cast<unsigned long>(timestamp_ms),
                      static_cast<unsigned long>(lap_count));
        RenderLapInfo(lap_count, last_valid_lap_ms, last_lap_delta_ms);
      }
    }
  }

  DEV_Delay_ms(25);
}
