#include "AMOLED_1in64.h"

void AMOLED_1in64_Init() {
  // Hardware init is handled by DEV_Module_Init and QSPI_PIO_Init.
}

void AMOLED_1in64_Clear(uint16_t color) {
  (void)color;
}

void AMOLED_1in64_ClearWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color) {
  (void)xStart;
  (void)yStart;
  (void)xEnd;
  (void)yEnd;
  (void)color;
}

void AMOLED_1in64_DrawString(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t size) {
  (void)x;
  (void)y;
  (void)text;
  (void)color;
  (void)background;
  (void)size;
}
