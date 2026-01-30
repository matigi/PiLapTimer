#pragma once

#include <stdint.h>

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

enum ReactionState {
  REACTION_IDLE,
  REACTION_ARMED,
  REACTION_COUNTDOWN,
  REACTION_READY_RANDOM,
  REACTION_WAIT_FOR_MOVE,
  REACTION_FALSE_START
};

struct ReactionUiSnapshot {
  ReactionState state;
  uint8_t amberCount;
  bool greenOn;
  bool reactionCaptured;
  uint32_t reactionMs;
  uint32_t bestReactionMs;
};

typedef void (*reaction_handler_t)(void);

void screen_reaction_attach(lv_obj_t *parent);
void screen_reaction_set_swipe_left_handler(reaction_handler_t cb);
void screen_reaction_set_swipe_right_handler(reaction_handler_t cb);
void screen_reaction_set_action_handler(reaction_handler_t cb);
void screen_reaction_set_arm_handler(reaction_handler_t cb);
void screen_reaction_update(const ReactionUiSnapshot &snapshot);
