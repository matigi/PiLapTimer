#ifndef SCREEN_GFORCE_H
#define SCREEN_GFORCE_H

#ifndef LV_CONF_INCLUDE_SIMPLE
#define LV_CONF_INCLUDE_SIMPLE
#endif
#include <lvgl.h>

void screen_gforce_init(void (*exitCb)());

void screen_gforce_show();
void screen_gforce_hide();

lv_obj_t *screen_gforce_get_screen();

#endif
