#include "lv_port_indev.h"

#include "FT3168.h"
#include "lv_port_disp.h"

static inline bool touchLooksInvalid(uint16_t x, uint16_t y) {
  if (x == 4095 && y == 4095) return true;
  if (x == 0xFFFF && y == 0xFFFF) return true;
  return false;
}

static bool readTouchSample(uint16_t &rawX, uint16_t &rawY) {
  if (!FT3168_Get_Point()) return false;

  uint16_t rx = (uint16_t)FT3168.x_point;
  uint16_t ry = (uint16_t)FT3168.y_point;
  if (touchLooksInvalid(rx, ry)) return false;

  rawX = rx;
  rawY = ry;
  return true;
}

static void normalizeTouch(uint16_t rawX, uint16_t rawY, uint16_t &nx, uint16_t &ny) {
  uint32_t x = rawX;
  uint32_t y = rawY;

  const uint16_t dispW = 280;
  const uint16_t dispH = 456;

  if (rawX > (dispW + 50) || rawY > (dispH + 50)) {
    x = (uint32_t)rawX * (dispW - 1) / 4095UL;
    y = (uint32_t)rawY * (dispH - 1) / 4095UL;
  }

  if (x >= dispW) x = dispW - 1;
  if (y >= dispH) y = dispH - 1;

  nx = (uint16_t)x;
  ny = (uint16_t)y;
}

static void rotateTouch(uint16_t nx, uint16_t ny, uint16_t &rx, uint16_t &ry) {
  rx = (LVGL_LOGICAL_W - 1) - ny;
  ry = nx;
}

static void lv_port_indev_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;
  static uint16_t lastX = 0;
  static uint16_t lastY = 0;

  uint16_t rawX = 0;
  uint16_t rawY = 0;
  if (readTouchSample(rawX, rawY)) {
    uint16_t normX = 0;
    uint16_t normY = 0;
    uint16_t rotX = 0;
    uint16_t rotY = 0;
    normalizeTouch(rawX, rawY, normX, normY);
    rotateTouch(normX, normY, rotX, rotY);
    lastX = rotX;
    lastY = rotY;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = lastX;
    data->point.y = lastY;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = lastX;
    data->point.y = lastY;
  }
}

void lv_port_indev_init() {
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_port_indev_read;
  lv_indev_drv_register(&indev_drv);
}
