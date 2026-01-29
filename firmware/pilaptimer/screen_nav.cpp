#include "screen_nav.h"

#include "lv_time_attack_ui.h"
#include "screen_gforce.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 1
#endif

#if USE_LVGL_UI
#include <lvgl.h>
namespace {
enum class ActiveScreen {
  Main,
  GForce
};

ActiveScreen gActiveScreen = ActiveScreen::Main;
}

void ShowMainScreen() {
  if (gActiveScreen == ActiveScreen::Main) return;
  screen_gforce_hide();
  lv_scr_load(lv_time_attack_ui_get_screen());
  gActiveScreen = ActiveScreen::Main;
}

void ShowGForceScreen() {
  if (gActiveScreen == ActiveScreen::GForce) return;
  screen_gforce_show();
  lv_scr_load(screen_gforce_get_screen());
  gActiveScreen = ActiveScreen::GForce;
}
#else
void ShowMainScreen() {}
void ShowGForceScreen() {}
#endif
