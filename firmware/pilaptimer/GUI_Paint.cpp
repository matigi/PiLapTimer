#include "GUI_Paint.h"

#include <stddef.h>

namespace {
struct Glyph {
  char symbol;
  uint8_t columns[5];
};

constexpr Glyph kGlyphs[] = {
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'a', {0x20, 0x54, 0x54, 0x54, 0x78}},
    {'e', {0x38, 0x54, 0x54, 0x54, 0x18}},
    {'i', {0x00, 0x44, 0x7D, 0x40, 0x00}},
    {'l', {0x00, 0x41, 0x7F, 0x40, 0x00}},
    {'m', {0x7C, 0x04, 0x18, 0x04, 0x78}},
    {'p', {0x7C, 0x14, 0x14, 0x14, 0x08}},
    {'r', {0x7C, 0x08, 0x04, 0x04, 0x08}},
    {'t', {0x04, 0x3F, 0x44, 0x40, 0x20}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
};

uint16_t *framebuffer = nullptr;
uint16_t canvasWidth = 0;
uint16_t canvasHeight = 0;

const Glyph *FindGlyph(char symbol) {
  for (const auto &glyph : kGlyphs) {
    if (glyph.symbol == symbol) {
      return &glyph;
    }
  }
  return nullptr;
}

void SetPixel(uint16_t x, uint16_t y, uint16_t color) {
  if (!framebuffer || x >= canvasWidth || y >= canvasHeight) {
    return;
  }
  framebuffer[y * canvasWidth + x] = color;
}

void DrawChar(uint16_t x, uint16_t y, char symbol, uint16_t color, uint16_t background, uint8_t size) {
  const Glyph *glyph = FindGlyph(symbol);
  const uint8_t *columns = glyph ? glyph->columns : nullptr;
  for (uint8_t col = 0; col < 5; ++col) {
    uint8_t columnData = columns ? columns[col] : 0x00;
    for (uint8_t row = 0; row < 7; ++row) {
      const bool isSet = columnData & (1U << row);
      const uint16_t drawColor = isSet ? color : background;
      for (uint8_t dx = 0; dx < size; ++dx) {
        for (uint8_t dy = 0; dy < size; ++dy) {
          SetPixel(x + col * size + dx, y + row * size + dy, drawColor);
        }
      }
    }
  }
}
}  // namespace

void Paint_NewImage(uint16_t *image, uint16_t width, uint16_t height, uint16_t rotate, uint16_t background) {
  (void)rotate;
  (void)background;
  framebuffer = image;
  canvasWidth = width;
  canvasHeight = height;
}

void Paint_Clear(uint16_t color) {
  if (!framebuffer) {
    return;
  }
  for (uint32_t i = 0; i < static_cast<uint32_t>(canvasWidth) * canvasHeight; ++i) {
    framebuffer[i] = color;
  }
}

void Paint_ClearWindows(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color) {
  if (!framebuffer) {
    return;
  }
  if (xEnd > canvasWidth) {
    xEnd = canvasWidth;
  }
  if (yEnd > canvasHeight) {
    yEnd = canvasHeight;
  }
  for (uint16_t y = yStart; y < yEnd; ++y) {
    for (uint16_t x = xStart; x < xEnd; ++x) {
      framebuffer[y * canvasWidth + x] = color;
    }
  }
}

void Paint_DrawString_EN(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t size) {
  if (!text) {
    return;
  }
  uint16_t cursorX = x;
  while (*text) {
    DrawChar(cursorX, y, *text, color, background, size);
    cursorX += static_cast<uint16_t>((5 + 1) * size);
    ++text;
  }
}

uint16_t *Paint_GetImage() {
  return framebuffer;
}

uint16_t Paint_GetWidth() {
  return canvasWidth;
}

uint16_t Paint_GetHeight() {
  return canvasHeight;
}
