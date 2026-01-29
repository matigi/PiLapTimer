#include "screen_nav.h"

#include "lv_time_attack_ui.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 1
#endif

#if USE_LVGL_UI
#include <lvgl.h>
namespace {
bool g_is_transitioning = false;
}

void ShowMainScreen() {
  lv_scr_load(lv_time_attack_ui_get_screen());
  lv_time_attack_ui_show_race_tile();
}

void ShowGForceScreen() {
  lv_scr_load(lv_time_attack_ui_get_screen());
  lv_time_attack_ui_show_gforce_tile();
}

void screen_nav_set_transitioning(bool transitioning) {
  g_is_transitioning = transitioning;
}

bool screen_nav_is_transitioning() { return g_is_transitioning; }
#else
void ShowMainScreen() {}
void ShowGForceScreen() {}
void screen_nav_set_transitioning(bool) {}
bool screen_nav_is_transitioning() { return false; }
#endif
