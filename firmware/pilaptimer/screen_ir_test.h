#ifndef SCREEN_IR_TEST_H
#define SCREEN_IR_TEST_H

#ifndef LV_CONF_INCLUDE_SIMPLE
#define LV_CONF_INCLUDE_SIMPLE
#endif
#include <lvgl.h>
#include <stdint.h>

struct IrTestUiSnapshot {
  bool beaconPresent;
  bool rawActivity;
  bool receiverLow;
  bool audioEnabled;
  uint16_t edgesLastWindow;
  uint16_t peakEdges;
  uint8_t onStreak;
  uint8_t offStreak;
  uint32_t totalEdges;
  uint32_t qualifiedEntries;
  uint32_t currentPresenceMs;
  uint32_t lastEdgeAgeMs;
  uint8_t hitPercent;
};

typedef void (*ir_test_action_handler_t)(void);

void screen_ir_test_attach(lv_obj_t *parent);
void screen_ir_test_set_audio_toggle_handler(ir_test_action_handler_t cb);
void screen_ir_test_set_reset_handler(ir_test_action_handler_t cb);
void screen_ir_test_update(const IrTestUiSnapshot &snapshot);

#endif
