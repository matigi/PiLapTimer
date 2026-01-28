#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifndef LV_CONF_INCLUDE_SIMPLE
#define LV_CONF_INCLUDE_SIMPLE
#endif
#include <lvgl.h>

static const uint16_t LVGL_LOGICAL_W = 456;
static const uint16_t LVGL_LOGICAL_H = 280;

void lv_port_disp_init();

#endif
