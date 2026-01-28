#include "lv_time_attack_ui.h"

#include <stdio.h>

namespace {
struct UiRefs {
  lv_obj_t *screen;
  lv_obj_t *settingsScreen;
  lv_obj_t *title;
  lv_obj_t *driver;
  lv_obj_t *lapsValue;
  lv_obj_t *lapTime;
  lv_obj_t *deltaPill;
  lv_obj_t *deltaLabel;
  lv_obj_t *bestLabel;
  lv_obj_t *lapLabel;
  lv_obj_t *sessionLabel;
  lv_obj_t *startBtn;
  lv_obj_t *startLabel;
  lv_obj_t *resetBtn;
  lv_obj_t *resetLabel;
  lv_obj_t *settingsBtn;
  lv_obj_t *settingsLabel;
  lv_obj_t *driverMinusBtn;
  lv_obj_t *driverPlusBtn;
  lv_obj_t *lapsMinusBtn;
  lv_obj_t *lapsPlusBtn;
  lv_obj_t *settingsBackBtn;
  lv_obj_t *settingsBackLabel;
};

UiRefs refs{};
void (*startStopHandler)() = nullptr;
void (*resetHandler)() = nullptr;
void (*driverPrevHandler)() = nullptr;
void (*driverNextHandler)() = nullptr;
void (*lapsPrevHandler)() = nullptr;
void (*lapsNextHandler)() = nullptr;
bool showingSettings = false;

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

void settings_open_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showingSettings = true;
    lv_scr_load(refs.settingsScreen);
  }
}

void settings_back_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showingSettings = false;
    lv_scr_load(refs.screen);
  }
}
}

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

  refs.screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(refs.screen, lv_color_hex(0x0b0f14), 0);
  lv_obj_set_style_bg_opa(refs.screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(refs.screen, LV_OBJ_FLAG_SCROLLABLE);

  refs.settingsScreen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(refs.settingsScreen, lv_color_hex(0x0b0f14), 0);
  lv_obj_set_style_bg_opa(refs.settingsScreen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(refs.settingsScreen, LV_OBJ_FLAG_SCROLLABLE);

  refs.title = lv_label_create(refs.screen);
  lv_label_set_text(refs.title, "Time Attack");
  lv_obj_set_style_text_color(refs.title, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(refs.title, &lv_font_montserrat_20, 0);
  lv_obj_align(refs.title, LV_ALIGN_TOP_LEFT, 16, 12);

  refs.settingsBtn = lv_btn_create(refs.screen);
  lv_obj_set_size(refs.settingsBtn, 108, 40);
  lv_obj_set_style_radius(refs.settingsBtn, 16, 0);
  lv_obj_set_style_bg_color(refs.settingsBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(refs.settingsBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.settingsBtn, 0, 0);
  lv_obj_align(refs.settingsBtn, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_obj_add_event_cb(refs.settingsBtn, settings_open_event, LV_EVENT_ALL, nullptr);

  refs.settingsLabel = lv_label_create(refs.settingsBtn);
  lv_label_set_text(refs.settingsLabel, "SETUP");
  lv_obj_set_style_text_font(refs.settingsLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refs.settingsLabel, lv_color_hex(0xe5edf7), 0);
  lv_obj_center(refs.settingsLabel);

  refs.lapTime = lv_label_create(refs.screen);
  lv_label_set_text(refs.lapTime, "--:--.---");
  lv_obj_set_style_text_color(refs.lapTime, lv_color_hex(0xf5f8ff), 0);
  lv_obj_set_style_text_font(refs.lapTime, &lv_font_montserrat_48, 0);
  lv_obj_align(refs.lapTime, LV_ALIGN_TOP_MID, 0, 58);

  refs.deltaPill = lv_obj_create(refs.screen);
  lv_obj_set_size(refs.deltaPill, 180, 42);
  lv_obj_set_style_radius(refs.deltaPill, 24, 0);
  lv_obj_set_style_bg_color(refs.deltaPill, lv_color_hex(0x1a2633), 0);
  lv_obj_set_style_bg_opa(refs.deltaPill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.deltaPill, 0, 0);
  lv_obj_align(refs.deltaPill, LV_ALIGN_TOP_MID, 0, 124);
  lv_obj_clear_flag(refs.deltaPill, LV_OBJ_FLAG_SCROLLABLE);

  refs.deltaLabel = lv_label_create(refs.deltaPill);
  lv_label_set_text(refs.deltaLabel, "Δ ---.---");
  lv_obj_set_style_text_color(refs.deltaLabel, lv_color_hex(0xdfe8f3), 0);
  lv_obj_set_style_text_font(refs.deltaLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(refs.deltaLabel);

  refs.bestLabel = lv_label_create(refs.screen);
  lv_label_set_text(refs.bestLabel, "Best --:--.---");
  lv_obj_set_style_text_color(refs.bestLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.bestLabel, &lv_font_montserrat_20, 0);
  lv_obj_align(refs.bestLabel, LV_ALIGN_TOP_LEFT, 16, 184);

  refs.lapLabel = lv_label_create(refs.screen);
  lv_label_set_text(refs.lapLabel, "Lap 0/0");
  lv_obj_set_style_text_color(refs.lapLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.lapLabel, &lv_font_montserrat_20, 0);
  lv_obj_align(refs.lapLabel, LV_ALIGN_TOP_RIGHT, -16, 184);

  refs.sessionLabel = lv_label_create(refs.screen);
  lv_label_set_text(refs.sessionLabel, "Session --:--.---");
  lv_obj_set_style_text_color(refs.sessionLabel, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(refs.sessionLabel, &lv_font_montserrat_20, 0);
  lv_obj_align(refs.sessionLabel, LV_ALIGN_TOP_MID, 0, 214);

  lv_obj_t *buttonRow = lv_obj_create(refs.screen);
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

  // Settings screen layout.
  lv_obj_t *settingsTitle = lv_label_create(refs.settingsScreen);
  lv_label_set_text(settingsTitle, "Setup");
  lv_obj_set_style_text_color(settingsTitle, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(settingsTitle, &lv_font_montserrat_24, 0);
  lv_obj_align(settingsTitle, LV_ALIGN_TOP_LEFT, 16, 12);

  auto make_settings_row = [](lv_obj_t *parent, const char *label_text,
                              lv_obj_t **minus_btn, lv_obj_t **value_label, lv_obj_t **plus_btn) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 420, 90);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    *minus_btn = lv_btn_create(row);
    lv_obj_set_size(*minus_btn, 84, 84);
    lv_obj_set_style_radius(*minus_btn, 20, 0);
    lv_obj_set_style_bg_color(*minus_btn, lv_color_hex(0x1e2a38), 0);
    lv_obj_set_style_bg_opa(*minus_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(*minus_btn, 0, 0);
    lv_obj_t *minus_label = lv_label_create(*minus_btn);
    lv_label_set_text(minus_label, "-");
    lv_obj_set_style_text_font(minus_label, &lv_font_montserrat_32, 0);
    lv_obj_center(minus_label);

    *value_label = lv_label_create(row);
    lv_label_set_text(*value_label, label_text);
    lv_obj_set_style_text_color(*value_label, lv_color_hex(0xe5edf7), 0);
    lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_32, 0);

    *plus_btn = lv_btn_create(row);
    lv_obj_set_size(*plus_btn, 84, 84);
    lv_obj_set_style_radius(*plus_btn, 20, 0);
    lv_obj_set_style_bg_color(*plus_btn, lv_color_hex(0x1e2a38), 0);
    lv_obj_set_style_bg_opa(*plus_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(*plus_btn, 0, 0);
    lv_obj_t *plus_label = lv_label_create(*plus_btn);
    lv_label_set_text(plus_label, "+");
    lv_obj_set_style_text_font(plus_label, &lv_font_montserrat_32, 0);
    lv_obj_center(plus_label);
    return row;
  };

  lv_obj_t *driverRow = make_settings_row(refs.settingsScreen,
                                          "Driver 1",
                                          &refs.driverMinusBtn,
                                          &refs.driver,
                                          &refs.driverPlusBtn);
  lv_obj_align(driverRow, LV_ALIGN_TOP_MID, 0, 70);

  lv_obj_t *lapsRow = make_settings_row(refs.settingsScreen,
                                        "Laps 5",
                                        &refs.lapsMinusBtn,
                                        &refs.lapsValue,
                                        &refs.lapsPlusBtn);
  lv_obj_align(lapsRow, LV_ALIGN_TOP_MID, 0, 180);

  lv_obj_add_event_cb(refs.driverMinusBtn, driver_minus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.driverPlusBtn, driver_plus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.lapsMinusBtn, laps_minus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.lapsPlusBtn, laps_plus_event, LV_EVENT_ALL, nullptr);

  refs.settingsBackBtn = lv_btn_create(refs.settingsScreen);
  lv_obj_set_size(refs.settingsBackBtn, 200, 64);
  lv_obj_set_style_radius(refs.settingsBackBtn, 18, 0);
  lv_obj_set_style_bg_color(refs.settingsBackBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(refs.settingsBackBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.settingsBackBtn, 1, 0);
  lv_obj_set_style_border_color(refs.settingsBackBtn, lv_color_hex(0x2e4052), 0);
  lv_obj_align(refs.settingsBackBtn, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_add_event_cb(refs.settingsBackBtn, settings_back_event, LV_EVENT_ALL, nullptr);

  refs.settingsBackLabel = lv_label_create(refs.settingsBackBtn);
  lv_label_set_text(refs.settingsBackLabel, "BACK");
  lv_obj_set_style_text_font(refs.settingsBackLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(refs.settingsBackLabel);

  lv_scr_load(refs.screen);
}

void lv_time_attack_ui_update(const UiSnapshot &snapshot) {
  char line[48];

  const bool idle = snapshot.state == UI_IDLE;
  if (idle) {
    lv_obj_clear_state(refs.driverMinusBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.driverPlusBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.lapsMinusBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.lapsPlusBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.settingsBtn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(refs.driverMinusBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.driverPlusBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.lapsMinusBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.lapsPlusBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.settingsBtn, LV_STATE_DISABLED);
  }

  snprintf(line, sizeof(line), "Driver %u", (unsigned)snapshot.selectedDriver);
  lv_label_set_text(refs.driver, line);
  snprintf(line, sizeof(line), "Laps %u", (unsigned)snapshot.selectedLaps);
  lv_label_set_text(refs.lapsValue, line);

  char timeBuf[24];
  formatTimeMaybe(timeBuf, sizeof(timeBuf), snapshot.state == UI_RUNNING || snapshot.state == UI_FINISHED,
                  snapshot.currentLapMs);
  lv_label_set_text(refs.lapTime, timeBuf);

  if (snapshot.lapCount > 0 && snapshot.bestLapMs > 0) {
    int32_t delta = snapshot.deltaMs;
    float deltaSec = (float)delta / 1000.0f;
    snprintf(line, sizeof(line), "Δ %+0.3fs", (double)deltaSec);
    lv_label_set_text(refs.deltaLabel, line);
    lv_color_t pillColor = (delta <= 0) ? lv_color_hex(0x1b5e3b) : lv_color_hex(0x5c1f28);
    lv_color_t textColor = (delta <= 0) ? lv_color_hex(0x8cf5be) : lv_color_hex(0xf6a3af);
    lv_obj_set_style_bg_color(refs.deltaPill, pillColor, 0);
    lv_obj_set_style_text_color(refs.deltaLabel, textColor, 0);
  } else {
    lv_label_set_text(refs.deltaLabel, "Δ ---.---");
    lv_obj_set_style_bg_color(refs.deltaPill, lv_color_hex(0x1a2633), 0);
    lv_obj_set_style_text_color(refs.deltaLabel, lv_color_hex(0xdfe8f3), 0);
  }

  formatTimeMaybe(timeBuf, sizeof(timeBuf), snapshot.bestLapMs > 0, snapshot.bestLapMs);
  snprintf(line, sizeof(line), "Best %s", timeBuf);
  lv_label_set_text(refs.bestLabel, line);

  uint8_t lapDisplay = snapshot.lapCount;
  if (snapshot.state == UI_RUNNING && lapDisplay < snapshot.selectedLaps) {
    lapDisplay++;
  }
  snprintf(line, sizeof(line), "Lap %u/%u", (unsigned)lapDisplay, (unsigned)snapshot.selectedLaps);
  lv_label_set_text(refs.lapLabel, line);

  formatTimeMaybe(timeBuf, sizeof(timeBuf), snapshot.sessionMs > 0, snapshot.sessionMs);
  snprintf(line, sizeof(line), "Session %s", timeBuf);
  lv_label_set_text(refs.sessionLabel, line);

  const bool running = snapshot.state == UI_RUNNING;
  lv_label_set_text(refs.startLabel, running ? "STOP" : "START");
  lv_obj_set_style_bg_color(refs.startBtn, running ? lv_color_hex(0xe05a63) : lv_color_hex(0x21c17a), 0);
  lv_obj_set_style_text_color(refs.startBtn, running ? lv_color_hex(0x2a0f12) : lv_color_hex(0x08140e), 0);

  if (running) {
    lv_obj_add_state(refs.startBtn, LV_STATE_DISABLED);
    lv_obj_add_state(refs.resetBtn, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(refs.startBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(refs.resetBtn, LV_STATE_DISABLED);
  }
}
