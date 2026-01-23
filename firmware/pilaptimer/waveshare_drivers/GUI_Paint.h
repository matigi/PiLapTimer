#ifndef GUI_PAINT_H
#define GUI_PAINT_H

#include <Arduino.h>

void Paint_Init(uint16_t width, uint16_t height);
void Paint_Clear(uint16_t color);
void Paint_ClearWindows(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void Paint_DrawString_EN(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t size);

#endif  // GUI_PAINT_H
