#include "screen_reaction.h"

#include <lvgl.h>
#include <stdio.h>

namespace {
struct ReactionRefs {
  lv_obj_t *root;
  lv_obj_t *statusLabel;
  lv_obj_t *rtLabel;
  lv_obj_t *timerLabel;
  lv_obj_t *bestLabel;
  lv_obj_t *lastLabel;
  lv_obj_t *armBtn;
  lv_obj_t *armLabel;
  lv_obj_t *amber[3];
  lv_obj_t *green;
};

ReactionRefs refs{};
reaction_handler_t swipeLeftHandler = nullptr;
reaction_handler_t swipeRightHandler = nullptr;
reaction_handler_t actionHandler = nullptr;
reaction_handler_t armHandler = nullptr;

void set_light(lv_obj_t *light, lv_color_t color, bool on) {
  lv_obj_set_style_bg_color(light, on ? color : lv_color_hex(0x202830), 0);
  lv_obj_set_style_bg_opa(light, on ? LV_OPA_COVER : LV_OPA_40, 0);
  lv_obj_set_style_shadow_color(light, color, 0);
  lv_obj_set_style_shadow_width(light, on ? 24 : 0, 0);
  lv_obj_set_style_shadow_opa(light, on ? LV_OPA_60 : LV_OPA_TRANSP, 0);
}

void update_status_style(ReactionState state) {
  lv_color_t color = lv_color_hex(0xbad1e8);
  if (state == REACTION_FALSE_START) {
    color = lv_color_hex(0xff5a5f);
  } else if (state == REACTION_GO_RUNNING || state == REACTION_RUN_ACTIVE) {
    color = lv_color_hex(0x7bf1a8);
  }
  lv_obj_set_style_text_color(refs.statusLabel, color, 0);
}

void format_ms(char *out, size_t size, uint32_t ms) {
  if (size == 0) return;
  float seconds = (float)ms / 1000.0f;
  snprintf(out, size, "%0.3fs", (double)seconds);
}

void on_root_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_LEFT && swipeLeftHandler) {
      swipeLeftHandler();
    } else if (dir == LV_DIR_RIGHT && swipeRightHandler) {
      swipeRightHandler();
    }
    return;
  }

  if (lv_event_get_code(e) == LV_EVENT_CLICKED && actionHandler) {
    actionHandler();
  }
}

void on_arm_btn_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && armHandler) {
    armHandler();
  }
}
}  // namespace

void screen_reaction_attach(lv_obj_t *parent) {
  refs.root = parent;
  lv_obj_set_style_bg_color(refs.root, lv_color_hex(0x0b0f14), 0);
  lv_obj_set_style_bg_opa(refs.root, LV_OPA_COVER, 0);
  lv_obj_clear_flag(refs.root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(refs.root, on_root_event, LV_EVENT_GESTURE, nullptr);
  lv_obj_add_event_cb(refs.root, on_root_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *title = lv_label_create(refs.root);
  lv_label_set_text(title, "REACTION RACE");
  lv_obj_set_style_text_color(title, lv_color_hex(0x8fa0b6), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  refs.statusLabel = lv_label_create(refs.root);
  lv_label_set_text(refs.statusLabel, "IDLE");
  lv_obj_set_style_text_font(refs.statusLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(refs.statusLabel, LV_ALIGN_TOP_MID, 0, 44);

  lv_obj_t *lightColumn = lv_obj_create(refs.root);
  lv_obj_set_size(lightColumn, 120, 260);
  lv_obj_set_style_bg_opa(lightColumn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(lightColumn, 0, 0);
  lv_obj_set_flex_flow(lightColumn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(lightColumn, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_align(lightColumn, LV_ALIGN_CENTER, 0, -10);
  lv_obj_clear_flag(lightColumn, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < 3; ++i) {
    refs.amber[i] = lv_obj_create(lightColumn);
    lv_obj_set_size(refs.amber[i], 70, 70);
    lv_obj_set_style_radius(refs.amber[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(refs.amber[i], 0, 0);
  }

  refs.green = lv_obj_create(lightColumn);
  lv_obj_set_size(refs.green, 70, 70);
  lv_obj_set_style_radius(refs.green, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(refs.green, 0, 0);

  refs.rtLabel = lv_label_create(refs.root);
  lv_label_set_text(refs.rtLabel, "RT: ---.---s");
  lv_obj_set_style_text_font(refs.rtLabel, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(refs.rtLabel, lv_color_hex(0xf5f8ff), 0);
  lv_obj_align(refs.rtLabel, LV_ALIGN_BOTTOM_MID, 0, -120);

  refs.timerLabel = lv_label_create(refs.root);
  lv_label_set_text(refs.timerLabel, "TIME: 0.000s");
  lv_obj_set_style_text_font(refs.timerLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(refs.timerLabel, lv_color_hex(0xc3d2e4), 0);
  lv_obj_align(refs.timerLabel, LV_ALIGN_BOTTOM_MID, 0, -90);

  refs.bestLabel = lv_label_create(refs.root);
  lv_label_set_text(refs.bestLabel, "Best RT: ---.---s");
  lv_obj_set_style_text_font(refs.bestLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refs.bestLabel, lv_color_hex(0x8fa0b6), 0);
  lv_obj_align(refs.bestLabel, LV_ALIGN_BOTTOM_LEFT, 16, -52);

  refs.lastLabel = lv_label_create(refs.root);
  lv_label_set_text(refs.lastLabel, "Last RT: ---.---s");
  lv_obj_set_style_text_font(refs.lastLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refs.lastLabel, lv_color_hex(0x8fa0b6), 0);
  lv_obj_align(refs.lastLabel, LV_ALIGN_BOTTOM_LEFT, 16, -28);

  refs.armBtn = lv_btn_create(refs.root);
  lv_obj_set_size(refs.armBtn, 200, 56);
  lv_obj_set_style_radius(refs.armBtn, 18, 0);
  lv_obj_set_style_bg_color(refs.armBtn, lv_color_hex(0x21c17a), 0);
  lv_obj_set_style_bg_opa(refs.armBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.armBtn, 0, 0);
  lv_obj_align(refs.armBtn, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_add_event_cb(refs.armBtn, on_arm_btn_event, LV_EVENT_CLICKED, nullptr);

  refs.armLabel = lv_label_create(refs.armBtn);
  lv_label_set_text(refs.armLabel, "ARM");
  lv_obj_set_style_text_font(refs.armLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(refs.armLabel);

  ReactionUiSnapshot snapshot{};
  snapshot.state = REACTION_IDLE;
  snapshot.amberCount = 0;
  snapshot.greenOn = false;
  snapshot.reactionCaptured = false;
  snapshot.reactionMs = 0;
  snapshot.runMs = 0;
  snapshot.bestReactionMs = 0;
  snapshot.lastReactionMs = 0;
  screen_reaction_update(snapshot);
}

void screen_reaction_set_swipe_left_handler(reaction_handler_t cb) {
  swipeLeftHandler = cb;
}

void screen_reaction_set_swipe_right_handler(reaction_handler_t cb) {
  swipeRightHandler = cb;
}

void screen_reaction_set_action_handler(reaction_handler_t cb) {
  actionHandler = cb;
}

void screen_reaction_set_arm_handler(reaction_handler_t cb) {
  armHandler = cb;
}

void screen_reaction_update(const ReactionUiSnapshot &snapshot) {
  const lv_color_t amberColor = lv_color_hex(0xffc857);
  for (int i = 0; i < 3; ++i) {
    set_light(refs.amber[i], amberColor, snapshot.amberCount > i);
  }
  set_light(refs.green, lv_color_hex(0x3ddc97), snapshot.greenOn);

  const char *stateText = "IDLE";
  switch (snapshot.state) {
    case REACTION_IDLE:
      stateText = "IDLE";
      break;
    case REACTION_ARMED:
      stateText = "ARMED";
      break;
    case REACTION_COUNTDOWN:
      stateText = "COUNTDOWN";
      break;
    case REACTION_READY_RANDOM:
      stateText = "READY";
      break;
    case REACTION_GO_RUNNING:
      stateText = "GO!";
      break;
    case REACTION_RUN_ACTIVE:
      stateText = "RUNNING";
      break;
    case REACTION_FALSE_START:
      stateText = "FALSE START";
      break;
  }
  lv_label_set_text(refs.statusLabel, stateText);
  update_status_style(snapshot.state);

  char buffer[32];
  if (snapshot.reactionCaptured) {
    format_ms(buffer, sizeof(buffer), snapshot.reactionMs);
    char line[40];
    snprintf(line, sizeof(line), "RT: %s", buffer);
    lv_label_set_text(refs.rtLabel, line);
  } else {
    lv_label_set_text(refs.rtLabel, "RT: ---.---s");
  }

  format_ms(buffer, sizeof(buffer), snapshot.runMs);
  char timeLine[40];
  snprintf(timeLine, sizeof(timeLine), "TIME: %s", buffer);
  lv_label_set_text(refs.timerLabel, timeLine);

  if (snapshot.bestReactionMs > 0) {
    format_ms(buffer, sizeof(buffer), snapshot.bestReactionMs);
    char bestLine[40];
    snprintf(bestLine, sizeof(bestLine), "Best RT: %s", buffer);
    lv_label_set_text(refs.bestLabel, bestLine);
  } else {
    lv_label_set_text(refs.bestLabel, "Best RT: ---.---s");
  }

  if (snapshot.lastReactionMs > 0) {
    format_ms(buffer, sizeof(buffer), snapshot.lastReactionMs);
    char lastLine[40];
    snprintf(lastLine, sizeof(lastLine), "Last RT: %s", buffer);
    lv_label_set_text(refs.lastLabel, lastLine);
  } else {
    lv_label_set_text(refs.lastLabel, "Last RT: ---.---s");
  }

  const bool armEnabled = snapshot.state == REACTION_IDLE || snapshot.state == REACTION_FALSE_START;
  lv_label_set_text(refs.armLabel, armEnabled ? "ARM" : "RESET");
  lv_obj_set_style_bg_color(refs.armBtn,
                            armEnabled ? lv_color_hex(0x21c17a) : lv_color_hex(0xe05a63),
                            0);
}
