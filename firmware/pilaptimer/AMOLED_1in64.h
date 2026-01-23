#ifndef AMOLED_1IN64_H
#define AMOLED_1IN64_H

#include <Arduino.h>

constexpr uint16_t AMOLED_1IN64_WIDTH = 280;
constexpr uint16_t AMOLED_1IN64_HEIGHT = 456;

void AMOLED_1in64_Init();
void AMOLED_1in64_Clear(uint16_t color);
void AMOLED_1in64_ClearWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void AMOLED_1in64_DrawString(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t size);

#endif  // AMOLED_1IN64_H
