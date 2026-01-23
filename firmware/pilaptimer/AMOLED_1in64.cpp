#include "AMOLED_1in64.h"
#include "GUI_Paint.h"

void AMOLED_1IN64_Init() {
  // Hardware init is handled by DEV_Module_Init, QSPI_GPIO_Init, and QSPI_PIO_Init.
}

void AMOLED_1IN64_SetBrightness(uint8_t brightness) {
  (void)brightness;
}

void AMOLED_1IN64_Display() {
  uint16_t *buffer = Paint_GetImage();
  const uint16_t width = Paint_GetWidth();
  const uint16_t height = Paint_GetHeight();
  (void)buffer;
  (void)width;
  (void)height;
  // Placeholder for full-frame display transfer.
}

void AMOLED_1IN64_Clear(uint16_t color) {
  Paint_Clear(color);
}

void AMOLED_1IN64_ClearWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color) {
  Paint_ClearWindows(xStart, yStart, xEnd, yEnd, color);
}
