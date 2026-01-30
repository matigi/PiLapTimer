#include "screen_reaction.h"

#include <lvgl.h>
#include <stdio.h>

namespace {
struct ReactionRefs {
  lv_obj_t *root;
  lv_obj_t *statusLabel;
  lv_obj_t *rtLabel;
  lv_obj_t *bestLabel;
  lv_obj_t *armBtn;
  lv_obj_t *armLabel;
  lv_obj_t *amberLeft[3];
  lv_obj_t *greenLeft;
  lv_obj_t *amberRight[3];
  lv_obj_t *greenRight;
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

void create_light_column(lv_obj_t *parent, lv_align_t align, int16_t xOffset, int16_t yOffset,
                         lv_obj_t *amber[3], lv_obj_t **green) {
  static constexpr int kLightSize = 60;
  static constexpr int kLightCount = 4;

  lv_obj_t *lightColumn = lv_obj_create(parent);
  lv_obj_set_size(lightColumn, kLightSize, kLightSize * kLightCount);
  lv_obj_set_style_bg_opa(lightColumn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(lightColumn, 0, 0);
  lv_obj_set_flex_flow(lightColumn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(lightColumn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(lightColumn, 0, 0);
  lv_obj_align(lightColumn, align, xOffset, yOffset);
  lv_obj_clear_flag(lightColumn, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < 3; ++i) {
    amber[i] = lv_obj_create(lightColumn);
    lv_obj_set_size(amber[i], kLightSize, kLightSize);
    lv_obj_set_style_radius(amber[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(amber[i], 0, 0);
  }

  *green = lv_obj_create(lightColumn);
  lv_obj_set_size(*green, kLightSize, kLightSize);
  lv_obj_set_style_radius(*green, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(*green, 0, 0);
}

void update_status_style(ReactionState state) {
  lv_color_t color = lv_color_hex(0xbad1e8);
  if (state == REACTION_FALSE_START) {
    color = lv_color_hex(0xff5a5f);
  } else if (state == REACTION_WAIT_FOR_MOVE) {
    color = lv_color_hex(0x7bf1a8);
  }
  lv_obj_set_style_text_color(refs.statusLabel, color, 0);
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
    if (lv_event_get_target(e) != refs.root) return;
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

  create_light_column(refs.root, LV_ALIGN_LEFT_MID, 16, -24, refs.amberLeft, &refs.greenLeft);
  create_light_column(refs.root, LV_ALIGN_RIGHT_MID, -16, -24, refs.amberRight, &refs.greenRight);

  refs.rtLabel = lv_label_create(refs.root);
  lv_label_set_text(refs.rtLabel, "R/T: ---.---s");
  lv_obj_set_style_text_font(refs.rtLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(refs.rtLabel, lv_color_hex(0xf5f8ff), 0);
  lv_obj_align(refs.rtLabel, LV_ALIGN_CENTER, 0, -48);

  refs.bestLabel = lv_label_create(refs.root);
  lv_label_set_text(refs.bestLabel, "Best R/T: ---.---s");
  lv_obj_set_style_text_font(refs.bestLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refs.bestLabel, lv_color_hex(0x8fa0b6), 0);
  lv_obj_align(refs.bestLabel, LV_ALIGN_CENTER, 0, 6);

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
  snapshot.armedCountdownSec = 0;
  snapshot.bestReactionMs = 0;
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
    set_light(refs.amberLeft[i], amberColor, snapshot.amberCount > i);
    set_light(refs.amberRight[i], amberColor, snapshot.amberCount > i);
  }
  set_light(refs.greenLeft, lv_color_hex(0x3ddc97), snapshot.greenOn);
  set_light(refs.greenRight, lv_color_hex(0x3ddc97), snapshot.greenOn);

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
    case REACTION_WAIT_FOR_MOVE:
      stateText = "WAITING FOR MOVE";
      break;
    case REACTION_FALSE_START:
      stateText = "FALSE START";
      break;
  }
  char stateLine[16];
  if (snapshot.state == REACTION_ARMED && snapshot.armedCountdownSec > 0) {
    snprintf(stateLine, sizeof(stateLine), "%u", (unsigned)snapshot.armedCountdownSec);
    lv_label_set_text(refs.statusLabel, stateLine);
  } else {
    lv_label_set_text(refs.statusLabel, stateText);
  }
  update_status_style(snapshot.state);

  char buffer[32];
  if (snapshot.reactionCaptured || snapshot.state == REACTION_WAIT_FOR_MOVE) {
    format_reaction_ms(buffer, sizeof(buffer), snapshot.reactionMs);
    char line[40];
    snprintf(line, sizeof(line), "R/T: %s", buffer);
    lv_label_set_text(refs.rtLabel, line);
  } else {
    lv_label_set_text(refs.rtLabel, "R/T: ---.---s");
  }

  if (snapshot.bestReactionMs > 0) {
    format_reaction_ms(buffer, sizeof(buffer), snapshot.bestReactionMs);
    char bestLine[40];
    snprintf(bestLine, sizeof(bestLine), "Best R/T: %s", buffer);
    lv_label_set_text(refs.bestLabel, bestLine);
  } else {
    lv_label_set_text(refs.bestLabel, "Best R/T: ---.---s");
  }

  const bool armEnabled = snapshot.state == REACTION_IDLE || snapshot.state == REACTION_FALSE_START;
  lv_label_set_text(refs.armLabel, armEnabled ? "ARM" : "RESET");
  lv_obj_set_style_bg_color(refs.armBtn,
                            armEnabled ? lv_color_hex(0x21c17a) : lv_color_hex(0xe05a63),
                            0);
}
