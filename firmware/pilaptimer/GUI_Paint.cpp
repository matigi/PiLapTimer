#include "GUI_Paint.h"
#include "AMOLED_1in64.h"

namespace {
uint16_t canvasWidth = 0;
uint16_t canvasHeight = 0;
}

void Paint_Init(uint16_t width, uint16_t height) {
  canvasWidth = width;
  canvasHeight = height;
  (void)canvasWidth;
  (void)canvasHeight;
}

void Paint_Clear(uint16_t color) {
  AMOLED_1in64_Clear(color);
}

void Paint_ClearWindows(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color) {
  AMOLED_1in64_ClearWindow(xStart, yStart, xEnd, yEnd, color);
}

void Paint_DrawString_EN(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t size) {
  AMOLED_1in64_DrawString(x, y, text, color, background, size);
}
