#ifndef GUI_PAINT_H
#define GUI_PAINT_H

#include <Arduino.h>

void Paint_NewImage(uint16_t *image, uint16_t width, uint16_t height, uint16_t rotate, uint16_t background);
void Paint_Clear(uint16_t color);
void Paint_ClearWindows(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void Paint_DrawString_EN(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t size);
uint16_t *Paint_GetImage();
uint16_t Paint_GetWidth();
uint16_t Paint_GetHeight();

#endif  // GUI_PAINT_H
