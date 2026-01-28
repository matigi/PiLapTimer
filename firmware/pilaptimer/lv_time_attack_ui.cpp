#include "lv_time_attack_ui.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr uint8_t kMaxLapHistory = 20;
constexpr uint8_t kChartPoints = 20;
constexpr uint8_t kMaxDrivers = 10;
constexpr uint8_t kMaxLaps = 20;
constexpr uint32_t kResetHoldMs = 800;
constexpr uint32_t kLapOverlayMs = 700;
constexpr uint32_t kBestIconMs = 1000;

struct LapEntry {
  uint8_t lapNumber;
  uint32_t lapMs;
  int32_t deltaMs;
};

struct UiRefs {
  lv_obj_t *screen;
  lv_obj_t *tileview;
  lv_obj_t *raceTile;
  lv_obj_t *reviewTile;
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
  lv_obj_t *settingsBtn;
  lv_obj_t *settingsLabel;

  lv_obj_t *lapOverlay;
  lv_obj_t *lapOverlayLap;
  lv_obj_t *lapOverlayTime;
  lv_obj_t *lapOverlayDelta;

  lv_obj_t *reviewBestValue;
  lv_obj_t *reviewAvgValue;
  lv_obj_t *reviewLastValue;
  lv_obj_t *lapTable;
  lv_obj_t *chart;
  lv_chart_series_t *chartSeries;

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

LapEntry lapHistory[kMaxLapHistory] = {};
uint8_t lapHistoryCount = 0;
uint32_t lapHistoryTotalMs = 0;
uint8_t lastLapCount = 0;

lv_timer_t *resetHoldTimer = nullptr;
lv_timer_t *overlayTimer = nullptr;
lv_timer_t *bestIconTimer = nullptr;
uint32_t resetHoldStart = 0;
bool resetTriggered = false;

lv_style_t bestRowStyle;

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

void hideOverlay(lv_timer_t *timer) {
  LV_UNUSED(timer);
  lv_obj_add_flag(refs.lapOverlay, LV_OBJ_FLAG_HIDDEN);
  if (overlayTimer) lv_timer_pause(overlayTimer);
}

void hideBestIcon(lv_timer_t *timer) {
  LV_UNUSED(timer);
  lv_obj_add_flag(refs.bestIcon, LV_OBJ_FLAG_HIDDEN);
  if (bestIconTimer) lv_timer_pause(bestIconTimer);
}

void resetLapHistory() {
  lapHistoryCount = 0;
  lapHistoryTotalMs = 0;
  lastLapCount = 0;
  for (uint8_t row = 1; row <= kMaxLapHistory; ++row) {
    lv_table_set_cell_value(refs.lapTable, row, 0, "");
    lv_table_set_cell_value(refs.lapTable, row, 1, "");
    lv_table_set_cell_value(refs.lapTable, row, 2, "");
  }
  for (uint8_t i = 0; i < kChartPoints; ++i) {
    lv_chart_set_value_by_id(refs.chart, refs.chartSeries, i, LV_CHART_POINT_NONE);
  }
  lv_chart_refresh(refs.chart);
}

void updateReviewStats(uint32_t bestLapMs, uint32_t lastLapMs) {
  char timeBuf[24];
  char line[32];

  formatTimeMaybe(timeBuf, sizeof(timeBuf), bestLapMs > 0, bestLapMs);
  snprintf(line, sizeof(line), "%s", timeBuf);
  lv_label_set_text(refs.reviewBestValue, line);

  if (lapHistoryCount > 0) {
    uint32_t avgMs = lapHistoryTotalMs / lapHistoryCount;
    formatTime(timeBuf, sizeof(timeBuf), avgMs);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "--:--.---");
  }
  lv_label_set_text(refs.reviewAvgValue, timeBuf);

  formatTimeMaybe(timeBuf, sizeof(timeBuf), lastLapMs > 0, lastLapMs);
  lv_label_set_text(refs.reviewLastValue, timeBuf);
}

void updateReviewTable(uint32_t bestLapMs) {
  int bestRow = -1;
  for (uint8_t i = 0; i < lapHistoryCount; ++i) {
    if (lapHistory[i].lapMs == bestLapMs && bestLapMs > 0) {
      bestRow = (int)i + 1;
      break;
    }
  }

  char line[24];
  char deltaBuf[16];
  for (uint8_t row = 1; row <= kMaxLapHistory; ++row) {
    bool isBest = (bestRow == (int)row);
    for (uint8_t col = 0; col < 3; ++col) {
      lv_table_clear_cell_ctrl(refs.lapTable, row, col, LV_TABLE_CELL_CTRL_CUSTOM_1);
      if (isBest) {
        lv_table_add_cell_ctrl(refs.lapTable, row, col, LV_TABLE_CELL_CTRL_CUSTOM_1);
      }
    }

    if (row <= lapHistoryCount) {
      const LapEntry &entry = lapHistory[row - 1];
      snprintf(line, sizeof(line), "%u", (unsigned)entry.lapNumber);
      lv_table_set_cell_value(refs.lapTable, row, 0, line);
      formatTime(line, sizeof(line), entry.lapMs);
      lv_table_set_cell_value(refs.lapTable, row, 1, line);
      formatDelta(deltaBuf, sizeof(deltaBuf), entry.deltaMs);
      lv_table_set_cell_value(refs.lapTable, row, 2, deltaBuf);
    } else {
      lv_table_set_cell_value(refs.lapTable, row, 0, "");
      lv_table_set_cell_value(refs.lapTable, row, 1, "");
      lv_table_set_cell_value(refs.lapTable, row, 2, "");
    }
  }
}

void updateReviewChart() {
  uint8_t startIndex = 0;
  if (lapHistoryCount > kChartPoints) {
    startIndex = lapHistoryCount - kChartPoints;
  }

  uint32_t minMs = 0;
  uint32_t maxMs = 0;
  bool hasPoint = false;
  for (uint8_t i = 0; i < kChartPoints; ++i) {
    uint8_t idx = startIndex + i;
    if (idx < lapHistoryCount) {
      uint32_t value = lapHistory[idx].lapMs;
      lv_chart_set_value_by_id(refs.chart, refs.chartSeries, i, value);
      if (!hasPoint) {
        minMs = value;
        maxMs = value;
        hasPoint = true;
      } else {
        if (value < minMs) minMs = value;
        if (value > maxMs) maxMs = value;
      }
    } else {
      lv_chart_set_value_by_id(refs.chart, refs.chartSeries, i, LV_CHART_POINT_NONE);
    }
  }

  if (!hasPoint) {
    lv_chart_set_range(refs.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 60000);
  } else {
    uint32_t padding = (maxMs - minMs) / 6 + 200;
    uint32_t minRange = (minMs > padding) ? (minMs - padding) : 0;
    uint32_t maxRange = maxMs + padding;
    if (maxRange < minRange + 1000) {
      maxRange = minRange + 1000;
    }
    lv_chart_set_range(refs.chart, LV_CHART_AXIS_PRIMARY_Y, minRange, maxRange);
  }
  lv_chart_refresh(refs.chart);
}

void showLapOverlay(uint8_t lapNumber, uint32_t lapMs, int32_t deltaMs) {
  char line[24];
  char deltaBuf[16];
  snprintf(line, sizeof(line), "LAP %u", (unsigned)lapNumber);
  lv_label_set_text(refs.lapOverlayLap, line);
  formatTime(line, sizeof(line), lapMs);
  lv_label_set_text(refs.lapOverlayTime, line);
  formatDelta(deltaBuf, sizeof(deltaBuf), deltaMs);
  snprintf(line, sizeof(line), "Δ %s", deltaBuf);
  lv_label_set_text(refs.lapOverlayDelta, line);
  lv_color_t deltaColor = (deltaMs <= 0) ? lv_color_hex(0x8cf5be) : lv_color_hex(0xf6a3af);
  lv_obj_set_style_text_color(refs.lapOverlayDelta, deltaColor, 0);
  lv_obj_clear_flag(refs.lapOverlay, LV_OBJ_FLAG_HIDDEN);
  if (overlayTimer) {
    lv_timer_reset(overlayTimer);
    lv_timer_resume(overlayTimer);
  }
}

void onNewLap(const UiSnapshot &snapshot) {
  LapEntry entry{};
  entry.lapNumber = snapshot.lapCount;
  entry.lapMs = snapshot.lastLapMs;
  entry.deltaMs = snapshot.deltaMs;

  if (lapHistoryCount < kMaxLapHistory) {
    lapHistory[lapHistoryCount++] = entry;
    lapHistoryTotalMs += entry.lapMs;
  } else {
    lapHistoryTotalMs -= lapHistory[0].lapMs;
    memmove(&lapHistory[0], &lapHistory[1], sizeof(LapEntry) * (kMaxLapHistory - 1));
    lapHistory[kMaxLapHistory - 1] = entry;
    lapHistoryTotalMs += entry.lapMs;
  }

  updateReviewStats(snapshot.bestLapMs, snapshot.lastLapMs);
  updateReviewTable(snapshot.bestLapMs);
  updateReviewChart();
  animateDeltaChip();
  showLapOverlay(snapshot.lapCount, snapshot.lastLapMs, snapshot.deltaMs);

  if (snapshot.bestLapMs == snapshot.lastLapMs && snapshot.bestLapMs > 0) {
    lv_obj_clear_flag(refs.bestIcon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align_to(refs.bestIcon, refs.bestLabel, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
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

void reset_hold_timer_cb(lv_timer_t *timer) {
  LV_UNUSED(timer);
  if (!lv_obj_has_state(refs.resetBtn, LV_STATE_PRESSED)) {
    lv_label_set_text(refs.resetLabel, "RESET");
    lv_timer_pause(resetHoldTimer);
    resetTriggered = false;
    return;
  }
  uint32_t elapsed = lv_tick_get() - resetHoldStart;
  if (elapsed >= kResetHoldMs && !resetTriggered) {
    resetTriggered = true;
    lv_label_set_text(refs.resetLabel, "RESET");
    if (resetHandler) resetHandler();
    lv_timer_pause(resetHoldTimer);
  } else {
    lv_label_set_text(refs.resetLabel, "HOLD");
  }
}

void reset_btn_event(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target(e);
  if (code == LV_EVENT_PRESSED) {
    animatePress(obj, true);
    resetHoldStart = lv_tick_get();
    resetTriggered = false;
    lv_label_set_text(refs.resetLabel, "HOLD");
    if (resetHoldTimer) {
      lv_timer_reset(resetHoldTimer);
      lv_timer_resume(resetHoldTimer);
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    animatePress(obj, false);
    if (!resetTriggered) {
      lv_label_set_text(refs.resetLabel, "RESET");
    }
    if (resetHoldTimer) {
      lv_timer_pause(resetHoldTimer);
    }
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
    lv_tileview_set_tile(refs.tileview, refs.settingsTile, LV_ANIM_ON);
  }
}

lv_obj_t *makeStatBlock(lv_obj_t *parent, const char *labelText, lv_obj_t **valueOut) {
  lv_obj_t *block = lv_obj_create(parent);
  lv_obj_set_size(block, 130, 56);
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(block, 0, 0);
  lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(block);
  lv_label_set_text(label, labelText);
  lv_obj_set_style_text_color(label, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 2);

  *valueOut = lv_label_create(block);
  lv_label_set_text(*valueOut, "--:--.---");
  lv_obj_set_style_text_color(*valueOut, lv_color_hex(0xe5edf7), 0);
  lv_obj_set_style_text_font(*valueOut, &lv_font_montserrat_20, 0);
  lv_obj_align(*valueOut, LV_ALIGN_BOTTOM_MID, 0, -2);

  return block;
}

lv_obj_t *makeSpinboxRow(lv_obj_t *parent, const char *labelText,
                         lv_obj_t **minusBtn, lv_obj_t **spinbox, lv_obj_t **plusBtn) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, 420, 72);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text(label, labelText);
  lv_obj_set_style_text_color(label, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);

  *minusBtn = lv_btn_create(row);
  lv_obj_set_size(*minusBtn, 56, 56);
  lv_obj_set_style_radius(*minusBtn, 16, 0);
  lv_obj_set_style_bg_color(*minusBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(*minusBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(*minusBtn, 0, 0);
  lv_obj_t *minusLabel = lv_label_create(*minusBtn);
  lv_label_set_text(minusLabel, "-");
  lv_obj_set_style_text_font(minusLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(minusLabel);

  *spinbox = lv_spinbox_create(row);
  lv_spinbox_set_range(*spinbox, 1, kMaxDrivers);
  lv_spinbox_set_digit_format(*spinbox, 2, 0);
  lv_spinbox_set_step(*spinbox, 1);
  lv_obj_set_width(*spinbox, 72);
  lv_obj_set_style_text_align(*spinbox, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(*spinbox, &lv_font_montserrat_24, 0);
  lv_obj_set_style_bg_color(*spinbox, lv_color_hex(0x0f151d), 0);
  lv_obj_set_style_bg_opa(*spinbox, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(*spinbox, 1, 0);
  lv_obj_set_style_border_color(*spinbox, lv_color_hex(0x2e4052), 0);
  lv_obj_clear_flag(*spinbox, LV_OBJ_FLAG_CLICKABLE);

  *plusBtn = lv_btn_create(row);
  lv_obj_set_size(*plusBtn, 56, 56);
  lv_obj_set_style_radius(*plusBtn, 16, 0);
  lv_obj_set_style_bg_color(*plusBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(*plusBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(*plusBtn, 0, 0);
  lv_obj_t *plusLabel = lv_label_create(*plusBtn);
  lv_label_set_text(plusLabel, "+");
  lv_obj_set_style_text_font(plusLabel, &lv_font_montserrat_24, 0);
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

  refs.settingsTile = lv_tileview_add_tile(refs.tileview, 0, 0, LV_DIR_RIGHT);
  refs.raceTile = lv_tileview_add_tile(refs.tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  refs.reviewTile = lv_tileview_add_tile(refs.tileview, 2, 0, LV_DIR_LEFT);

  lv_tileview_set_tile(refs.tileview, refs.raceTile, LV_ANIM_OFF);

  // Race tile
  refs.bestLabel = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.bestLabel, "BEST --:--.---");
  lv_obj_set_style_text_color(refs.bestLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.bestLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_width(refs.bestLabel, 200);
  lv_label_set_long_mode(refs.bestLabel, LV_LABEL_LONG_CLIP);
  lv_obj_align(refs.bestLabel, LV_ALIGN_TOP_LEFT, 16, 12);

  refs.bestIcon = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.bestIcon, LV_SYMBOL_STAR);
  lv_obj_set_style_text_color(refs.bestIcon, lv_color_hex(0xffd166), 0);
  lv_obj_set_style_text_font(refs.bestIcon, &lv_font_montserrat_20, 0);
  lv_obj_add_flag(refs.bestIcon, LV_OBJ_FLAG_HIDDEN);

  refs.lapLabel = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.lapLabel, "LAP 0/0");
  lv_obj_set_style_text_color(refs.lapLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.lapLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_width(refs.lapLabel, 140);
  lv_label_set_long_mode(refs.lapLabel, LV_LABEL_LONG_CLIP);
  lv_obj_align(refs.lapLabel, LV_ALIGN_TOP_RIGHT, -16, 12);

  refs.settingsBtn = lv_btn_create(refs.raceTile);
  lv_obj_set_size(refs.settingsBtn, 46, 46);
  lv_obj_set_style_radius(refs.settingsBtn, 14, 0);
  lv_obj_set_style_bg_color(refs.settingsBtn, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(refs.settingsBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.settingsBtn, 0, 0);
  lv_obj_align(refs.settingsBtn, LV_ALIGN_TOP_RIGHT, -10, 46);
  lv_obj_add_event_cb(refs.settingsBtn, settings_open_event, LV_EVENT_ALL, nullptr);

  refs.settingsLabel = lv_label_create(refs.settingsBtn);
  lv_label_set_text(refs.settingsLabel, LV_SYMBOL_SETTINGS);
  lv_obj_set_style_text_font(refs.settingsLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(refs.settingsLabel, lv_color_hex(0xe5edf7), 0);
  lv_obj_center(refs.settingsLabel);

  refs.lapTime = lv_label_create(refs.raceTile);
  lv_label_set_text(refs.lapTime, "--:--.---");
  lv_obj_set_style_text_color(refs.lapTime, lv_color_hex(0xf5f8ff), 0);
  lv_obj_set_style_text_font(refs.lapTime, &lv_font_montserrat_48, 0);
  lv_obj_set_width(refs.lapTime, 320);
  lv_label_set_long_mode(refs.lapTime, LV_LABEL_LONG_CLIP);
  lv_obj_align(refs.lapTime, LV_ALIGN_TOP_MID, 0, 64);
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
  lv_obj_center(refs.deltaLabel);

  refs.lapOverlay = lv_obj_create(refs.raceTile);
  lv_obj_set_size(refs.lapOverlay, 220, 120);
  lv_obj_set_style_radius(refs.lapOverlay, 18, 0);
  lv_obj_set_style_bg_color(refs.lapOverlay, lv_color_hex(0x16212d), 0);
  lv_obj_set_style_bg_opa(refs.lapOverlay, LV_OPA_90, 0);
  lv_obj_set_style_border_width(refs.lapOverlay, 1, 0);
  lv_obj_set_style_border_color(refs.lapOverlay, lv_color_hex(0x2e4052), 0);
  lv_obj_center(refs.lapOverlay);
  lv_obj_add_flag(refs.lapOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(refs.lapOverlay, LV_OBJ_FLAG_SCROLLABLE);

  refs.lapOverlayLap = lv_label_create(refs.lapOverlay);
  lv_label_set_text(refs.lapOverlayLap, "LAP 1");
  lv_obj_set_style_text_font(refs.lapOverlayLap, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refs.lapOverlayLap, lv_color_hex(0x8fa0b6), 0);
  lv_obj_align(refs.lapOverlayLap, LV_ALIGN_TOP_MID, 0, 10);

  refs.lapOverlayTime = lv_label_create(refs.lapOverlay);
  lv_label_set_text(refs.lapOverlayTime, "0:00.000");
  lv_obj_set_style_text_font(refs.lapOverlayTime, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(refs.lapOverlayTime, lv_color_hex(0xffffff), 0);
  lv_obj_align(refs.lapOverlayTime, LV_ALIGN_CENTER, 0, -2);

  refs.lapOverlayDelta = lv_label_create(refs.lapOverlay);
  lv_label_set_text(refs.lapOverlayDelta, "Δ +0.000");
  lv_obj_set_style_text_font(refs.lapOverlayDelta, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refs.lapOverlayDelta, lv_color_hex(0x9fd4ff), 0);
  lv_obj_align(refs.lapOverlayDelta, LV_ALIGN_BOTTOM_MID, 0, -12);

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

  // Review tile
  lv_obj_t *reviewContainer = lv_obj_create(refs.reviewTile);
  lv_obj_set_size(reviewContainer, 456, 280);
  lv_obj_set_style_bg_opa(reviewContainer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(reviewContainer, 0, 0);
  lv_obj_set_flex_flow(reviewContainer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(reviewContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(reviewContainer, 10, 0);
  lv_obj_clear_flag(reviewContainer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *statsRow = lv_obj_create(reviewContainer);
  lv_obj_set_size(statsRow, 420, 60);
  lv_obj_set_style_bg_opa(statsRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(statsRow, 0, 0);
  lv_obj_set_flex_flow(statsRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(statsRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(statsRow, LV_OBJ_FLAG_SCROLLABLE);

  makeStatBlock(statsRow, "BEST", &refs.reviewBestValue);
  makeStatBlock(statsRow, "AVG", &refs.reviewAvgValue);
  makeStatBlock(statsRow, "LAST", &refs.reviewLastValue);

  refs.lapTable = lv_table_create(reviewContainer);
  lv_obj_set_size(refs.lapTable, 420, 140);
  lv_table_set_col_cnt(refs.lapTable, 3);
  lv_table_set_row_cnt(refs.lapTable, kMaxLapHistory + 1);
  lv_table_set_col_width(refs.lapTable, 0, 60);
  lv_table_set_col_width(refs.lapTable, 1, 200);
  lv_table_set_col_width(refs.lapTable, 2, 120);
  lv_obj_set_style_text_font(refs.lapTable, &lv_font_montserrat_16, 0);
  lv_obj_set_style_border_width(refs.lapTable, 0, 0);
  lv_obj_set_style_bg_color(refs.lapTable, lv_color_hex(0x0f151d), 0);
  lv_obj_set_style_bg_opa(refs.lapTable, LV_OPA_COVER, 0);
  lv_obj_add_style(refs.lapTable, &bestRowStyle, LV_PART_ITEMS | LV_STATE_USER_1);
  lv_table_set_cell_value(refs.lapTable, 0, 0, "LAP");
  lv_table_set_cell_value(refs.lapTable, 0, 1, "TIME");
  lv_table_set_cell_value(refs.lapTable, 0, 2, "Δ");

  refs.chart = lv_chart_create(reviewContainer);
  lv_obj_set_size(refs.chart, 420, 70);
  lv_chart_set_type(refs.chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(refs.chart, kChartPoints);
  lv_chart_set_range(refs.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 60000);
  lv_obj_set_style_bg_color(refs.chart, lv_color_hex(0x0f151d), 0);
  lv_obj_set_style_bg_opa(refs.chart, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.chart, 0, 0);
  lv_obj_set_style_line_color(refs.chart, lv_color_hex(0x6cc8ff), LV_PART_ITEMS);
  refs.chartSeries = lv_chart_add_series(refs.chart, lv_color_hex(0x6cc8ff), LV_CHART_AXIS_PRIMARY_Y);

  // Settings tile
  lv_obj_t *settingsTitle = lv_label_create(refs.settingsTile);
  lv_label_set_text(settingsTitle, "SETTINGS");
  lv_obj_set_style_text_color(settingsTitle, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(settingsTitle, &lv_font_montserrat_24, 0);
  lv_obj_align(settingsTitle, LV_ALIGN_TOP_LEFT, 16, 12);

  lv_obj_t *driverRow = makeSpinboxRow(refs.settingsTile, "Driver #",
                                       &refs.driverMinusBtn, &refs.driverSpinbox, &refs.driverPlusBtn);
  lv_obj_align(driverRow, LV_ALIGN_TOP_MID, 0, 60);

  lv_obj_t *lapsRow = makeSpinboxRow(refs.settingsTile, "Laps",
                                     &refs.lapsMinusBtn, &refs.lapsSpinbox, &refs.lapsPlusBtn);
  lv_obj_align(lapsRow, LV_ALIGN_TOP_MID, 0, 140);
  lv_spinbox_set_range(refs.lapsSpinbox, 1, kMaxLaps);

  lv_obj_add_event_cb(refs.driverMinusBtn, driver_minus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.driverPlusBtn, driver_plus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.lapsMinusBtn, laps_minus_event, LV_EVENT_ALL, nullptr);
  lv_obj_add_event_cb(refs.lapsPlusBtn, laps_plus_event, LV_EVENT_ALL, nullptr);

  resetHoldTimer = lv_timer_create(reset_hold_timer_cb, 50, nullptr);
  lv_timer_pause(resetHoldTimer);

  overlayTimer = lv_timer_create(hideOverlay, kLapOverlayMs, nullptr);
  lv_timer_pause(overlayTimer);

  bestIconTimer = lv_timer_create(hideBestIcon, kBestIconMs, nullptr);
  lv_timer_pause(bestIconTimer);

  lv_scr_load(refs.screen);
}

void lv_time_attack_ui_update(const UiSnapshot &snapshot) {
  char line[48];

  bool idle = snapshot.state == UI_IDLE;
  if (idle) {
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
                  snapshot.currentLapMs);
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

  if (snapshot.lapCount < lastLapCount ||
      (snapshot.lapCount == 0 && snapshot.lastLapMs == 0 && snapshot.bestLapMs == 0 && lapHistoryCount > 0)) {
    resetLapHistory();
  }

  if (snapshot.lapCount > lastLapCount) {
    onNewLap(snapshot);
    lastLapCount = snapshot.lapCount;
  } else if (snapshot.lapCount == 0) {
    lastLapCount = snapshot.lapCount;
  }

  updateReviewStats(snapshot.bestLapMs, snapshot.lastLapMs);
  updateReviewTable(snapshot.bestLapMs);
  updateReviewChart();
}
