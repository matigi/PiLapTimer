#include <Arduino.h>

#include "AMOLED_1in64.h"
#include "DEV_Config.h"
#include "FT3168.h"
#include "GUI_Paint.h"
#include "fonts.h"
#include "qspi_pio.h"

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
static bool last_touch_active = false;
static uint16_t last_touch_x = 0;
static uint16_t last_touch_y = 0;
}  // namespace

void RenderTouchInfo(bool touch_active, uint16_t x, uint16_t y) {
  Paint_SelectImage(reinterpret_cast<UBYTE *>(info_image));
  Paint_Clear(BLACK);
  Paint_DrawString_EN(8, 6, "Touch Input", &Font20, WHITE, BLACK);

  if (touch_active) {
    char x_text[16];
    char y_text[16];
    snprintf(x_text, sizeof(x_text), "X: %03u", x);
    snprintf(y_text, sizeof(y_text), "Y: %03u", y);
    Paint_DrawString_EN(8, 32, x_text, &Font20, GREEN, BLACK);
    Paint_DrawString_EN(8, 54, y_text, &Font20, GREEN, BLACK);
  } else {
    Paint_DrawString_EN(8, 38, "No touch", &Font16, RED, BLACK);
  }

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

  Serial.println("Touch init");
  FT3168_Init(FT3168_Point_Mode);

  Paint_NewImage(reinterpret_cast<UBYTE *>(info_image),
                 AMOLED_1IN64_WIDTH, kInfoHeight, ROTATE_0, BLACK);
  Paint_SetScale(65);
  Paint_SetMirroring(MIRROR_NONE);
  RenderTouchInfo(false, 0, 0);

  Serial.println("DONE");
}

void loop() {
  bool touch_active = FT3168_Get_Point();
  uint16_t touch_x = FT3168.x_point;
  uint16_t touch_y = FT3168.y_point;

  if (touch_active != last_touch_active ||
      (touch_active &&
       (touch_x != last_touch_x || touch_y != last_touch_y))) {
    RenderTouchInfo(touch_active, touch_x, touch_y);
    last_touch_active = touch_active;
    last_touch_x = touch_x;
    last_touch_y = touch_y;
  }

  DEV_Delay_ms(25);
}
