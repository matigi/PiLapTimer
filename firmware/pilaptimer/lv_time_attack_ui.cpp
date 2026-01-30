#include "lv_time_attack_ui.h"

#include <stdio.h>
#include <string.h>

#include "screen_gforce.h"
#include "screen_reaction.h"

namespace {
constexpr uint8_t kMaxDrivers = 10;
constexpr uint8_t kMaxLaps = 20;
constexpr uint32_t kBestIconMs = 1000;

struct UiRefs {
  lv_obj_t *screen;
  lv_obj_t *tileview;
  lv_obj_t *raceTile;
  lv_obj_t *reactionTile;
  lv_obj_t *reviewTile;
  lv_obj_t *gforceTile;
  lv_obj_t *settingsTile;

  lv_obj_t *bestLabel;
  lv_obj_t *bestIcon;
  lv_obj_t *lapLabel;
  lv_obj_t *lapTime;
  lv_obj_t *deltaPill;
  lv_obj_t *deltaLabel;
  lv_obj_t *startBtn;
  lv_obj_t *startLabel;
  lv_obj_t *resetBtn;
  lv_obj_t *resetLabel;

  lv_obj_t *reportTable;

  lv_obj_t *driverMinusBtn;
  lv_obj_t *driverPlusBtn;
  lv_obj_t *driverSpinbox;
  lv_obj_t *lapsMinusBtn;
  lv_obj_t *lapsPlusBtn;
  lv_obj_t *lapsSpinbox;
};

UiRefs refs{};
void (*startStopHandler)() = nullptr;
void (*resetHandler)() = nullptr;
void (*driverPrevHandler)() = nullptr;
void (*driverNextHandler)() = nullptr;
void (*lapsPrevHandler)() = nullptr;
void (*lapsNextHandler)() = nullptr;
nav_handler_t swipeLeftHandler = nullptr;
nav_handler_t swipeRightHandler = nullptr;

lv_style_t bestRowStyle;

lv_timer_t *bestIconTimer = nullptr;
uint8_t lastLapCount = 0;


void formatTime(char *out, size_t outSize, uint32_t ms) {
  if (outSize == 0) return;
  uint32_t totalSec = ms / 1000;
  uint32_t minutes = totalSec / 60;
  uint32_t seconds = totalSec % 60;
  uint32_t millisPart = ms % 1000;
  snprintf(out, outSize, "%lu:%02lu.%03lu",
           (unsigned long)minutes,
           (unsigned long)seconds,
           (unsigned long)millisPart);
}

void formatTimeMaybe(char *out, size_t outSize, bool valid, uint32_t ms) {
  if (!valid) {
    snprintf(out, outSize, "--:--.---");
    return;
  }
  formatTime(out, outSize, ms);
}

void formatDelta(char *out, size_t outSize, int32_t deltaMs) {
  float deltaSec = (float)deltaMs / 1000.0f;
  snprintf(out, outSize, "%+0.3f", (double)deltaSec);
}

void setZoom(void *obj, int32_t v) {
  lv_obj_set_style_transform_zoom(static_cast<lv_obj_t *>(obj), v, 0);
}

void animatePress(lv_obj_t *obj, bool pressed) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, pressed ? 256 : 240, pressed ? 240 : 256);
  lv_anim_set_time(&a, 120);
  lv_anim_set_exec_cb(&a, setZoom);
  lv_anim_start(&a);
}

void animateDeltaChip() {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, refs.deltaPill);
  lv_anim_set_values(&a, 256, 300);
  lv_anim_set_time(&a, 180);
  lv_anim_set_playback_time(&a, 180);
  lv_anim_set_exec_cb(&a, setZoom);
  lv_anim_start(&a);

  lv_anim_t b;
  lv_anim_init(&b);
  lv_anim_set_var(&b, refs.deltaLabel);
  lv_anim_set_values(&b, 256, 300);
  lv_anim_set_time(&b, 180);
  lv_anim_set_playback_time(&b, 180);
  lv_anim_set_exec_cb(&b, setZoom);
  lv_anim_start(&b);
}

void hideBestIcon(lv_timer_t *timer) {
  LV_UNUSED(timer);
  lv_obj_add_flag(refs.bestIcon, LV_OBJ_FLAG_HIDDEN);
  if (bestIconTimer) lv_timer_pause(bestIconTimer);
}

void onNewLap(const UiSnapshot &snapshot) {
  animateDeltaChip();

  if (snapshot.bestLapMs == snapshot.lastLapMs && snapshot.bestLapMs > 0) {
    lv_obj_clear_flag(refs.bestIcon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align_to(refs.bestIcon, refs.bestLabel, LV_ALIGN_OUT_RIGHT_MID, -10, 0);
    if (bestIconTimer) {
      lv_timer_reset(bestIconTimer);
      lv_timer_resume(bestIconTimer);
    }
  }
}

void start_btn_event(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target(e);
  if (code == LV_EVENT_PRESSED) {
    animatePress(obj, true);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    animatePress(obj, false);
  } else if (code == LV_EVENT_CLICKED) {
    if (startStopHandler) startStopHandler();
  }
}

void reset_btn_event(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target(e);
  if (code == LV_EVENT_PRESSED) {
    animatePress(obj, true);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    animatePress(obj, false);
  } else if (code == LV_EVENT_CLICKED) {
    if (resetHandler) resetHandler();
  }
}

void driver_minus_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && driverPrevHandler) {
    driverPrevHandler();
  }
}

void driver_plus_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && driverNextHandler) {
    driverNextHandler();
  }
}

void laps_minus_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && lapsPrevHandler) {
    lapsPrevHandler();
  }
}

void laps_plus_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && lapsNextHandler) {
    lapsNextHandler();
  }
}

void screen_gesture_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) return;
  lv_dir_t dir = lv_indev_get_gesture_dir(indev);
  if (dir == LV_DIR_LEFT && swipeLeftHandler) {
    swipeLeftHandler();
  } else if (dir == LV_DIR_RIGHT && swipeRightHandler) {
    swipeRightHandler();
  }
}

void tileview_scroll_event(lv_event_t *e) {
  LV_UNUSED(e);
}

lv_obj_t *makeSpinboxRow(lv_obj_t *parent, const char *labelText,
                         lv_obj_t **minusBtn, lv_obj_t **spinbox, lv_obj_t **plusBtn) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, 420, 80);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(row, 12, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text(label, labelText);
  lv_obj_set_style_text_color(label, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_set_width(label, 140);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);

  *minusBtn = lv_btn_create(row);
  lv_obj_set_size(*minusBtn, 68, 68);
  lv_obj_set_style_radius(*minusBtn, 18, 0);
  lv_obj_set_style_bg_color(*minusBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(*minusBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(*minusBtn, 0, 0);
  lv_obj_t *minusLabel = lv_label_create(*minusBtn);
  lv_label_set_text(minusLabel, "-");
  lv_obj_set_style_text_font(minusLabel, &lv_font_montserrat_32, 0);
  lv_obj_center(minusLabel);

  *spinbox = lv_spinbox_create(row);
  lv_spinbox_set_range(*spinbox, 1, kMaxDrivers);
  lv_spinbox_set_digit_format(*spinbox, 2, 0);
  lv_spinbox_set_step(*spinbox, 1);
  lv_obj_set_size(*spinbox, 80, 56);
  lv_obj_set_style_text_align(*spinbox, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(*spinbox, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(*spinbox, lv_color_hex(0xf5f8ff), 0);
  lv_obj_set_style_bg_opa(*spinbox, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(*spinbox, 0, 0);
  lv_obj_set_style_pad_all(*spinbox, 0, 0);
  lv_obj_set_style_pad_row(*spinbox, 0, 0);
  lv_obj_set_style_bg_opa(*spinbox, LV_OPA_TRANSP, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(*spinbox, 0, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_PRESSED);
  lv_obj_remove_style(*spinbox, NULL, LV_PART_CURSOR);
  lv_obj_clear_flag(*spinbox, LV_OBJ_FLAG_CLICKABLE);

  *plusBtn = lv_btn_create(row);
  lv_obj_set_size(*plusBtn, 68, 68);
  lv_obj_set_style_radius(*plusBtn, 18, 0);
  lv_obj_set_style_bg_color(*plusBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(*plusBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(*plusBtn, 0, 0);
  lv_obj_t *plusLabel = lv_label_create(*plusBtn);
  lv_label_set_text(plusLabel, "+");
  lv_obj_set_style_text_font(plusLabel, &lv_font_montserrat_32, 0);
  lv_obj_center(plusLabel);

  return row;
}
}  // namespace

void lv_time_attack_ui_init(void (*startStopCb)(),
                            void (*resetCb)(),
                            void (*driverPrevCb)(),
                            void (*driverNextCb)(),
                            void (*lapsPrevCb)(),
                            void (*lapsNextCb)()) {
  startStopHandler = startStopCb;
  resetHandler = resetCb;
  driverPrevHandler = driverPrevCb;
  driverNextHandler = driverNextCb;
  lapsPrevHandler = lapsPrevCb;
  lapsNextHandler = lapsNextCb;

  lv_style_init(&bestRowStyle);
  lv_style_set_bg_color(&bestRowStyle, lv_color_hex(0x1f2f3f));
  lv_style_set_bg_opa(&bestRowStyle, LV_OPA_COVER);
  lv_style_set_text_color(&bestRowStyle, lv_color_hex(0xffe6a3));

  refs.screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(refs.screen, lv_color_hex(0x0b0f14), 0);
  lv_obj_set_style_bg_opa(refs.screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(refs.screen, LV_OBJ_FLAG_SCROLLABLE);

  refs.tileview = lv_tileview_create(refs.screen);
  lv_obj_set_size(refs.tileview, lv_disp_get_hor_res(nullptr), lv_disp_get_ver_res(nullptr));
  lv_obj_set_style_bg_opa(refs.tileview, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(refs.tileview, 0, 0);
  lv_obj_set_scrollbar_mode(refs.tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(refs.tileview, tileview_scroll_event, LV_EVENT_SCROLL_BEGIN, nullptr);
  lv_obj_add_event_cb(refs.tileview, tileview_scroll_event, LV_EVENT_SCROLL_END, nullptr);

  refs.settingsTile = lv_tileview_add_tile(refs.tileview, 0, 0, LV_DIR_RIGHT);
  refs.raceTile = lv_tileview_add_tile(refs.tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  refs.reactionTile = lv_tileview_add_tile(refs.tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  refs.gforceTile = lv_tileview_add_tile(refs.tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  refs.reviewTile = lv_tileview_add_tile(refs.tileview, 4, 0, LV_DIR_LEFT);

  lv_obj_set_tile(refs.tileview, refs.raceTile, LV_ANIM_OFF);
  // Swipe left on the main timer tile to reach Reaction Race (G-Force is one more swipe).
  // Swipe right on the main timer tile to reach Settings.
  lv_obj_add_event_cb(refs.raceTile, screen_gesture_event, LV_EVENT_GESTURE, nullptr);
  screen_gforce_attach(refs.gforceTile);
  screen_reaction_attach(refs.reactionTile);

  // Race tile
  refs.bestLabel = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.bestLabel, "BEST --:--.---");
  lv_obj_set_style_text_color(refs.bestLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.bestLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_width(refs.bestLabel, 240);
  lv_label_set_long_mode(refs.bestLabel, LV_LABEL_LONG_CLIP);
  lv_obj_align(refs.bestLabel, LV_ALIGN_TOP_LEFT, 16, 12);

  refs.bestIcon = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.bestIcon, LV_SYMBOL_OK);
  lv_obj_set_style_text_color(refs.bestIcon, lv_color_hex(0xffd166), 0);
  lv_obj_set_style_text_font(refs.bestIcon, &lv_font_montserrat_20, 0);
  lv_obj_add_flag(refs.bestIcon, LV_OBJ_FLAG_HIDDEN);

  refs.lapLabel = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.lapLabel, "LAP 0/0");
  lv_obj_set_style_text_color(refs.lapLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.lapLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_width(refs.lapLabel, 220);
  lv_obj_set_style_text_align(refs.lapLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(refs.lapLabel, LV_LABEL_LONG_CLIP);
  lv_obj_align(refs.lapLabel, LV_ALIGN_TOP_RIGHT, -16, 12);

  refs.lapTime = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.lapTime, "--:--.---");
  lv_obj_set_style_text_color(refs.lapTime, lv_color_hex(0xf5f8ff), 0);
  lv_obj_set_style_text_font(refs.lapTime, &lv_font_montserrat_48, 0);
  lv_obj_set_width(refs.lapTime, 320);
  lv_label_set_long_mode(refs.lapTime, LV_LABEL_LONG_CLIP);
  lv_obj_align(refs.lapTime, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_set_style_text_align(refs.lapTime, LV_TEXT_ALIGN_CENTER, 0);

  refs.deltaPill = lv_obj_create(refs.raceTile);
  lv_obj_set_size(refs.deltaPill, 180, 44);
  lv_obj_set_style_radius(refs.deltaPill, 22, 0);
  lv_obj_set_style_bg_color(refs.deltaPill, lv_color_hex(0x1a2633), 0);
  lv_obj_set_style_bg_opa(refs.deltaPill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.deltaPill, 0, 0);
  lv_obj_align(refs.deltaPill, LV_ALIGN_TOP_MID, 0, 124);
  lv_obj_clear_flag(refs.deltaPill, LV_OBJ_FLAG_SCROLLABLE);

  refs.deltaLabel = lv_label_create(refs.deltaPill);
  lv_label_set_text(refs.deltaLabel, "---.---");
  lv_obj_set_style_text_color(refs.deltaLabel, lv_color_hex(0xdfe8f3), 0);
  lv_obj_set_style_text_font(refs.deltaLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_width(refs.deltaLabel, 160);
  lv_obj_set_style_text_align(refs.deltaLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(refs.deltaLabel);

  lv_obj_t *buttonRow = lv_obj_create(refs.raceTile);
  lv_obj_set_size(buttonRow, 420, 72);
  lv_obj_align(buttonRow, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_bg_opa(buttonRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(buttonRow, 0, 0);
  lv_obj_set_flex_flow(buttonRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(buttonRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(buttonRow, LV_OBJ_FLAG_SCROLLABLE);

  refs.startBtn = lv_btn_create(buttonRow);
  lv_obj_set_size(refs.startBtn, 200, 64);
  lv_obj_set_style_radius(refs.startBtn, 18, 0);
  lv_obj_set_style_bg_color(refs.startBtn, lv_color_hex(0x21c17a), 0);
  lv_obj_set_style_bg_opa(refs.startBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(refs.startBtn, 12, 0);
  lv_obj_set_style_shadow_color(refs.startBtn, lv_color_hex(0x0b2f1f), 0);
  lv_obj_set_style_shadow_opa(refs.startBtn, LV_OPA_50, 0);
  lv_obj_set_style_text_color(refs.startBtn, lv_color_hex(0x08140e), 0);
  lv_obj_add_event_cb(refs.startBtn, start_btn_event, LV_EVENT_ALL, nullptr);

  refs.startLabel = lv_label_create(refs.startBtn);
  lv_label_set_text(refs.startLabel, "START");
  lv_obj_set_style_text_font(refs.startLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(refs.startLabel);

  refs.resetBtn = lv_btn_create(buttonRow);
  lv_obj_set_size(refs.resetBtn, 200, 64);
  lv_obj_set_style_radius(refs.resetBtn, 18, 0);
  lv_obj_set_style_bg_color(refs.resetBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(refs.resetBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.resetBtn, 1, 0);
  lv_obj_set_style_border_color(refs.resetBtn, lv_color_hex(0x2e4052), 0);
  lv_obj_set_style_shadow_width(refs.resetBtn, 10, 0);
  lv_obj_set_style_shadow_color(refs.resetBtn, lv_color_hex(0x0b1118), 0);
  lv_obj_set_style_shadow_opa(refs.resetBtn, LV_OPA_50, 0);
  lv_obj_set_style_text_color(refs.resetBtn, lv_color_hex(0xe5edf7), 0);
  lv_obj_add_event_cb(refs.resetBtn, reset_btn_event, LV_EVENT_ALL, nullptr);

  refs.resetLabel = lv_label_create(refs.resetBtn);
  lv_label_set_text(refs.resetLabel, "RESET");
  lv_obj_set_style_text_font(refs.resetLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(refs.resetLabel);

  lv_obj_set_style_transform_pivot_x(refs.startBtn, 100, 0);
  lv_obj_set_style_transform_pivot_y(refs.startBtn, 32, 0);
  lv_obj_set_style_transform_pivot_x(refs.resetBtn, 100, 0);
  lv_obj_set_style_transform_pivot_y(refs.resetBtn, 32, 0);

  // Reporting tile
  lv_obj_t *reviewContainer = lv_obj_create(refs.reviewTile);
  lv_obj_set_size(reviewContainer, lv_disp_get_hor_res(nullptr), lv_disp_get_ver_res(nullptr));
  lv_obj_set_style_bg_opa(reviewContainer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(reviewContainer, 0, 0);
  lv_obj_set_flex_flow(reviewContainer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(reviewContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(reviewContainer, 12, 0);
  lv_obj_clear_flag(reviewContainer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *reportTitle = lv_label_create(reviewContainer);
  lv_label_set_text(reportTitle, "REPORTING");
  lv_obj_set_style_text_color(reportTitle, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(reportTitle, &lv_font_montserrat_20, 0);

  refs.reportTable = lv_table_create(reviewContainer);
  lv_obj_set_size(refs.reportTable, 420, 210);
  lv_table_set_col_cnt(refs.reportTable, 4);
  lv_table_set_row_cnt(refs.reportTable, kMaxDrivers + 1);
  lv_table_set_col_width(refs.reportTable, 0, 60);
  lv_table_set_col_width(refs.reportTable, 1, 140);
  lv_table_set_col_width(refs.reportTable, 2, 120);
  lv_table_set_col_width(refs.reportTable, 3, 100);
  lv_obj_set_style_text_font(refs.reportTable, &lv_font_montserrat_14, 0);
  lv_obj_set_style_border_width(refs.reportTable, 0, 0);
  lv_obj_set_style_bg_color(refs.reportTable, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(refs.reportTable, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(refs.reportTable, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_color(refs.reportTable, lv_color_hex(0xffffff), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(refs.reportTable, lv_color_hex(0x000000), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(refs.reportTable, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_width(refs.reportTable, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(refs.reportTable, lv_color_hex(0xffffff), LV_PART_ITEMS);
  lv_table_set_cell_value(refs.reportTable, 0, 0, "DRV");
  lv_table_set_cell_value(refs.reportTable, 0, 1, "BEST TOTAL");
  lv_table_set_cell_value(refs.reportTable, 0, 2, "BEST LAP");
  lv_table_set_cell_value(refs.reportTable, 0, 3, "BEST RT");

  // Settings tile
  lv_obj_t *settingsTitle = lv_label_create(refs.settingsTile);
  lv_label_set_text(settingsTitle, "SETTINGS");
  lv_obj_set_style_text_color(settingsTitle, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(settingsTitle, &lv_font_montserrat_24, 0);
  lv_obj_align(settingsTitle, LV_ALIGN_TOP_LEFT, 16, 12);

  lv_obj_t *settingsContainer = lv_obj_create(refs.settingsTile);
  lv_obj_set_size(settingsContainer, 420, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(settingsContainer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settingsContainer, 0, 0);
  lv_obj_set_flex_flow(settingsContainer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(settingsContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(settingsContainer, 16, 0);
  lv_obj_clear_flag(settingsContainer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(settingsContainer, LV_ALIGN_TOP_MID, 0, 56);

  (void)makeSpinboxRow(settingsContainer, "DRIVER",
                       &refs.driverMinusBtn, &refs.driverSpinbox, &refs.driverPlusBtn);

  (void)makeSpinboxRow(settingsContainer, "LAPS",
                       &refs.lapsMinusBtn, &refs.lapsSpinbox, &refs.lapsPlusBtn);
  lv_spinbox_set_range(refs.lapsSpinbox, 1, kMaxLaps);

  lv_obj_add_event_cb(refs.driverMinusBtn, driver_minus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.driverPlusBtn, driver_plus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.lapsMinusBtn, laps_minus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.lapsPlusBtn, laps_plus_event, LV_EVENT_ALL, nullptr);

  bestIconTimer = lv_timer_create(hideBestIcon, kBestIconMs, nullptr);
  lv_timer_pause(bestIconTimer);

  lv_scr_load(refs.screen);
}

void lv_time_attack_ui_set_swipe_left_handler(nav_handler_t cb) {
  swipeLeftHandler = cb;
}

void lv_time_attack_ui_set_swipe_right_handler(nav_handler_t cb) {
  swipeRightHandler = cb;
}

lv_obj_t *lv_time_attack_ui_get_screen() {
  return refs.screen;
}

void lv_time_attack_ui_show_race_tile() {
  if (!refs.tileview || !refs.raceTile) return;
  lv_obj_set_tile(refs.tileview, refs.raceTile, LV_ANIM_ON);
}

void lv_time_attack_ui_show_reaction_tile() {
  if (!refs.tileview || !refs.reactionTile) return;
  lv_obj_set_tile(refs.tileview, refs.reactionTile, LV_ANIM_ON);
}

void lv_time_attack_ui_show_settings_tile() {
  if (!refs.tileview || !refs.settingsTile) return;
  lv_obj_set_tile(refs.tileview, refs.settingsTile, LV_ANIM_ON);
}

void lv_time_attack_ui_show_gforce_tile() {
  if (!refs.tileview || !refs.gforceTile) return;
  lv_obj_set_tile(refs.tileview, refs.gforceTile, LV_ANIM_ON);
}

void lv_time_attack_ui_update(const UiSnapshot &snapshot) {
  char line[48];

  bool controlsEnabled = snapshot.state != UI_RUNNING;
  if (controlsEnabled) {
    lv_obj_clear_state(refs.driverMinusBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.driverPlusBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.lapsMinusBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.lapsPlusBtn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(refs.driverMinusBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.driverPlusBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.lapsMinusBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.lapsPlusBtn, LV_STATE_DISABLED);
  }

  lv_spinbox_set_value(refs.driverSpinbox, snapshot.selectedDriver);
  lv_spinbox_set_value(refs.lapsSpinbox, snapshot.selectedLaps);

  char timeBuf[24];
  formatTimeMaybe(timeBuf, sizeof(timeBuf), snapshot.bestLapMs > 0, snapshot.bestLapMs);
  snprintf(line, sizeof(line), "BEST %s", timeBuf);
  lv_label_set_text(refs.bestLabel, line);
  lv_obj_align_to(refs.bestIcon, refs.bestLabel, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

  uint8_t lapDisplay = snapshot.lapCount;
  if (snapshot.state == UI_RUNNING && lapDisplay < snapshot.selectedLaps) {
    lapDisplay++;
  }
  snprintf(line, sizeof(line), "LAP %u/%u", (unsigned)lapDisplay, (unsigned)snapshot.selectedLaps);
  lv_label_set_text(refs.lapLabel, line);

  formatTimeMaybe(timeBuf, sizeof(timeBuf), snapshot.state == UI_RUNNING || snapshot.state == UI_FINISHED,
                  snapshot.sessionMs);
  lv_label_set_text(refs.lapTime, timeBuf);

  if (snapshot.lapCount > 0 && snapshot.bestLapMs > 0) {
    formatDelta(line, sizeof(line), snapshot.deltaMs);
    lv_label_set_text(refs.deltaLabel, line);
    lv_color_t pillColor = (snapshot.deltaMs <= 0) ? lv_color_hex(0x1b5e3b) : lv_color_hex(0x5c1f28);
    lv_color_t textColor = (snapshot.deltaMs <= 0) ? lv_color_hex(0x8cf5be) : lv_color_hex(0xf6a3af);
    lv_obj_set_style_bg_color(refs.deltaPill, pillColor, 0);
    lv_obj_set_style_text_color(refs.deltaLabel, textColor, 0);
  } else {
    lv_label_set_text(refs.deltaLabel, "---.---");
    lv_obj_set_style_bg_color(refs.deltaPill, lv_color_hex(0x1a2633), 0);
    lv_obj_set_style_text_color(refs.deltaLabel, lv_color_hex(0xdfe8f3), 0);
  }

  const bool running = snapshot.state == UI_RUNNING || snapshot.state == UI_ARMED;
  lv_label_set_text(refs.startLabel, running ? "STOP" : "START");
  lv_obj_set_style_bg_color(refs.startBtn, running ? lv_color_hex(0xe05a63) : lv_color_hex(0x21c17a), 0);
  lv_obj_set_style_text_color(refs.startBtn, running ? lv_color_hex(0x2a0f12) : lv_color_hex(0x08140e), 0);

  if (snapshot.state == UI_RUNNING) {
    lv_obj_add_state(refs.resetBtn, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(refs.resetBtn, LV_STATE_DISABLED);
  }

  if (snapshot.lapCount > lastLapCount) {
    onNewLap(snapshot);
  }
  lastLapCount = snapshot.lapCount;

  for (uint8_t i = 0; i < kMaxDrivers; ++i) {
    uint8_t row = i + 1;
    snprintf(line, sizeof(line), "%u", (unsigned)(i + 1));
    lv_table_set_cell_value(refs.reportTable, row, 0, line);
    if (snapshot.driverRunValid[i]) {
      formatTime(timeBuf, sizeof(timeBuf), snapshot.driverTotalMs[i]);
      lv_table_set_cell_value(refs.reportTable, row, 1, timeBuf);
      formatTime(timeBuf, sizeof(timeBuf), snapshot.driverBestLapMs[i]);
      lv_table_set_cell_value(refs.reportTable, row, 2, timeBuf);
    } else {
      lv_table_set_cell_value(refs.reportTable, row, 1, "--");
      lv_table_set_cell_value(refs.reportTable, row, 2, "--");
    }

    if (snapshot.driverBestReactionMs[i] > 0) {
      format_reaction_ms(timeBuf, sizeof(timeBuf), snapshot.driverBestReactionMs[i]);
      lv_table_set_cell_value(refs.reportTable, row, 3, timeBuf);
    } else {
      lv_table_set_cell_value(refs.reportTable, row, 3, "--");
    }
  }
}
