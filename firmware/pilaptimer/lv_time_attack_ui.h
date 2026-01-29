#ifndef LV_TIME_ATTACK_UI_H
#define LV_TIME_ATTACK_UI_H

#ifndef LV_CONF_INCLUDE_SIMPLE
#define LV_CONF_INCLUDE_SIMPLE
#endif
#include <lvgl.h>
#include "ui_state.h"

struct UiSnapshot {
  UiState state;
  uint8_t selectedDriver;
  uint8_t selectedLaps;
  uint8_t lapCount;
  uint32_t sessionMs;
  uint32_t currentLapMs;
  uint32_t lastLapMs;
  uint32_t bestLapMs;
  int32_t deltaMs;
  bool driverRunValid[10];
  uint32_t driverTotalMs[10];
  uint32_t driverBestLapMs[10];
};

void lv_time_attack_ui_init(void (*startStopCb)(),
                            void (*resetCb)(),
                            void (*driverPrevCb)(),
                            void (*driverNextCb)(),
                            void (*lapsPrevCb)(),
                            void (*lapsNextCb)());
typedef void (*nav_handler_t)(void);
void lv_time_attack_ui_set_swipe_down_handler(nav_handler_t cb);
void lv_time_attack_ui_update(const UiSnapshot &snapshot);
lv_obj_t *lv_time_attack_ui_get_screen();

#endif
