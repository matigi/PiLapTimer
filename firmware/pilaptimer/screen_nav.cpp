#include "screen_nav.h"

#include "lv_time_attack_ui.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 1
#endif

#if USE_LVGL_UI
#include <lvgl.h>
namespace {
constexpr uint32_t kNavAnimMs = 300;
bool g_isTransitioning = false;
lv_timer_t *g_transitionTimer = nullptr;

void end_transition(lv_timer_t *timer) {
  LV_UNUSED(timer);
  g_isTransitioning = false;
  if (g_transitionTimer) {
    lv_timer_del(g_transitionTimer);
    g_transitionTimer = nullptr;
  }
}

void start_transition() {
  g_isTransitioning = true;
  if (g_transitionTimer) {
    lv_timer_del(g_transitionTimer);
    g_transitionTimer = nullptr;
  }
  g_transitionTimer = lv_timer_create(end_transition, kNavAnimMs, nullptr);
}
}  // namespace

void ShowMainScreen() {
  start_transition();
  if (lv_scr_act() != lv_time_attack_ui_get_screen()) {
    lv_scr_load_anim(lv_time_attack_ui_get_screen(),
                     LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                     kNavAnimMs,
                     0,
                     false);
  }
  lv_time_attack_ui_show_race_tile();
}

void ShowGForceScreen() {
  start_transition();
  if (lv_scr_act() != lv_time_attack_ui_get_screen()) {
    lv_scr_load_anim(lv_time_attack_ui_get_screen(),
                     LV_SCR_LOAD_ANIM_MOVE_LEFT,
                     kNavAnimMs,
                     0,
                     false);
  }
  lv_time_attack_ui_show_gforce_tile();
}

void screen_nav_set_transitioning(bool transitioning) {
  g_isTransitioning = transitioning;
  if (!transitioning && g_transitionTimer) {
    lv_timer_del(g_transitionTimer);
    g_transitionTimer = nullptr;
  }
}

bool screen_nav_is_transitioning() {
  return g_isTransitioning;
}
#else
void ShowMainScreen() {}
void ShowGForceScreen() {}
void screen_nav_set_transitioning(bool) {}
bool screen_nav_is_transitioning() { return false; }
#endif
