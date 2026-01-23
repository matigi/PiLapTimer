#ifndef AMOLED_1IN64_H
#define AMOLED_1IN64_H

#include <Arduino.h>

constexpr uint16_t AMOLED_1IN64_WIDTH = 280;
constexpr uint16_t AMOLED_1IN64_HEIGHT = 456;

void AMOLED_1IN64_Init();
void AMOLED_1IN64_SetBrightness(uint8_t brightness);
void AMOLED_1IN64_Display();
void AMOLED_1IN64_Clear(uint16_t color);
void AMOLED_1IN64_ClearWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color);

#endif  // AMOLED_1IN64_H
