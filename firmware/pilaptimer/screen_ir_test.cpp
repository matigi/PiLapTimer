#include "screen_ir_test.h"

#include <stdio.h>

namespace {
struct IrTestRefs {
  lv_obj_t *statusPill;
  lv_obj_t *statusLabel;
  lv_obj_t *edgeLabel;
  lv_obj_t *signalBar;
  lv_obj_t *detailLabel;
  lv_obj_t *statsLabel;
  lv_obj_t *audioButton;
  lv_obj_t *audioLabel;
  lv_obj_t *resetButton;
};

IrTestRefs refs{};
ir_test_action_handler_t audioToggleHandler = nullptr;
ir_test_action_handler_t resetHandler = nullptr;

void button_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_obj_t *target = lv_event_get_target(event);
  if (target == refs.audioButton) {
    if (audioToggleHandler) audioToggleHandler();
  } else if (target == refs.resetButton) {
    if (resetHandler) resetHandler();
  }
}

lv_obj_t *make_button(lv_obj_t *parent, const char *text, int16_t x) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, 196, 52);
  lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, x, -12);
  lv_obj_set_style_radius(button, 16, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x1e2a38), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lv_color_hex(0x33465b), 0);
  lv_obj_add_event_cb(button, button_event, LV_EVENT_ALL, nullptr);

  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xf5f8ff), 0);
  lv_obj_center(label);
  return button;
}
}  // namespace

void screen_ir_test_attach(lv_obj_t *parent) {
  if (!parent) return;

  lv_obj_t *title = lv_label_create(parent);
  lv_label_set_text(title, "IR BEAM TEST");
  lv_obj_set_style_text_color(title, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 12);

  refs.statusPill = lv_obj_create(parent);
  lv_obj_set_size(refs.statusPill, 156, 42);
  lv_obj_align(refs.statusPill, LV_ALIGN_TOP_RIGHT, -16, 8);
  lv_obj_set_style_radius(refs.statusPill, 21, 0);
  lv_obj_set_style_border_width(refs.statusPill, 0, 0);
  lv_obj_set_style_bg_opa(refs.statusPill, LV_OPA_COVER, 0);
  lv_obj_clear_flag(refs.statusPill, LV_OBJ_FLAG_SCROLLABLE);

  refs.statusLabel = lv_label_create(refs.statusPill);
  lv_obj_set_style_text_font(refs.statusLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(refs.statusLabel);

  refs.edgeLabel = lv_label_create(parent);
  lv_label_set_text(refs.edgeLabel, "0 EDGES / 20 ms");
  lv_obj_set_style_text_color(refs.edgeLabel, lv_color_hex(0xf5f8ff), 0);
  lv_obj_set_style_text_font(refs.edgeLabel, &lv_font_montserrat_32, 0);
  lv_obj_align(refs.edgeLabel, LV_ALIGN_TOP_LEFT, 16, 64);

  refs.signalBar = lv_bar_create(parent);
  lv_obj_set_size(refs.signalBar, 270, 22);
  lv_obj_align(refs.signalBar, LV_ALIGN_TOP_LEFT, 16, 108);
  lv_bar_set_range(refs.signalBar, 0, 32);
  lv_obj_set_style_radius(refs.signalBar, 11, LV_PART_MAIN);
  lv_obj_set_style_radius(refs.signalBar, 11, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(refs.signalBar, lv_color_hex(0x1a2633), LV_PART_MAIN);
  lv_obj_set_style_bg_color(refs.signalBar, lv_color_hex(0x21c17a), LV_PART_INDICATOR);

  refs.detailLabel = lv_label_create(parent);
  lv_label_set_text(refs.detailLabel, "RAW --  ON 0  OFF 0");
  lv_obj_set_style_text_color(refs.detailLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.detailLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(refs.detailLabel, LV_ALIGN_TOP_LEFT, 16, 140);

  refs.statsLabel = lv_label_create(parent);
  lv_label_set_text(refs.statsLabel, "PEAK 0   HITS 0\nEDGES 0  HIT 0%  AGE --");
  lv_obj_set_width(refs.statsLabel, 420);
  lv_obj_set_style_text_color(refs.statsLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_set_style_text_font(refs.statsLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_line_space(refs.statsLabel, 5, 0);
  lv_obj_align(refs.statsLabel, LV_ALIGN_TOP_LEFT, 16, 169);

  refs.audioButton = make_button(parent, "AUDIO ON", 16);
  refs.audioLabel = lv_obj_get_child(refs.audioButton, 0);
  refs.resetButton = make_button(parent, "RESET STATS", 244);
}

void screen_ir_test_set_audio_toggle_handler(ir_test_action_handler_t cb) {
  audioToggleHandler = cb;
}

void screen_ir_test_set_reset_handler(ir_test_action_handler_t cb) {
  resetHandler = cb;
}

void screen_ir_test_update(const IrTestUiSnapshot &snapshot) {
  if (!refs.statusLabel) return;

  lv_label_set_text(refs.statusLabel, snapshot.beaconPresent ? "DETECTED" : "NO SIGNAL");
  lv_obj_set_style_bg_color(refs.statusPill,
                            snapshot.beaconPresent ? lv_color_hex(0x17633f)
                                                   : lv_color_hex(0x5c1f28),
                            0);
  lv_obj_set_style_text_color(refs.statusLabel,
                              snapshot.beaconPresent ? lv_color_hex(0x9df4c5)
                                                     : lv_color_hex(0xf6a3af),
                              0);

  char line[96];
  snprintf(line, sizeof(line), "%u EDGES / 20 ms",
           (unsigned)snapshot.edgesLastWindow);
  lv_label_set_text(refs.edgeLabel, line);

  int32_t barValue = snapshot.edgesLastWindow;
  if (barValue > 32) barValue = 32;
  lv_bar_set_value(refs.signalBar, barValue, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(refs.signalBar,
                            snapshot.rawActivity ? lv_color_hex(0x21c17a)
                                                 : lv_color_hex(0xd49a2a),
                            LV_PART_INDICATOR);

  snprintf(line, sizeof(line), "RAW %s  PIN %s  ON %u  OFF %u",
           snapshot.rawActivity ? "ACTIVE" : "quiet",
           snapshot.receiverLow ? "LOW" : "HIGH",
           (unsigned)snapshot.onStreak,
           (unsigned)snapshot.offStreak);
  lv_label_set_text(refs.detailLabel, line);

  char age[16];
  if (snapshot.lastEdgeAgeMs > 99999UL) {
    snprintf(age, sizeof(age), "--");
  } else {
    snprintf(age, sizeof(age), "%lums", (unsigned long)snapshot.lastEdgeAgeMs);
  }
  snprintf(line, sizeof(line),
           "PEAK %u   HITS %lu   ON %lums\nEDGES %lu  HIT %u%%  AGE %s",
           (unsigned)snapshot.peakEdges,
           (unsigned long)snapshot.qualifiedEntries,
           (unsigned long)snapshot.currentPresenceMs,
           (unsigned long)snapshot.totalEdges,
           (unsigned)snapshot.hitPercent,
           age);
  lv_label_set_text(refs.statsLabel, line);

  lv_label_set_text(refs.audioLabel, snapshot.audioEnabled ? "AUDIO ON" : "AUDIO OFF");
  lv_obj_set_style_bg_color(refs.audioButton,
                            snapshot.audioEnabled ? lv_color_hex(0x17633f)
                                                  : lv_color_hex(0x1e2a38),
                            0);
}
