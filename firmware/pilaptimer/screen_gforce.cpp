#include "screen_gforce.h"

#include <math.h>
#include <stdio.h>

#include <Arduino.h>

#include "imu_qmi8658.h"
#include "screen_nav.h"

static lv_obj_t *gforceScreen = nullptr;

namespace {
constexpr float kGravityMs2 = 9.80665f;
constexpr uint32_t kCalibrationMs = 800;
constexpr float kGravityAlpha = 0.02f;
constexpr float kSmoothingAlpha = 0.25f;
constexpr float kSmoothingAlphaStill = 0.6f;
constexpr float kStillThreshold = 0.05f;
constexpr float kPeakEpsilon = 0.01f;
constexpr uint32_t kUpdateMs = 30;

constexpr int32_t kCircleSize = 170;
constexpr int32_t kBallSize = 14;
constexpr int32_t kTickLength = 10;
constexpr int32_t kSmallTickLength = 6;

// Axis mapping for the installed PCB orientation.
// Adjust these indexes/signs after a real-world test if forward/left do not match.
constexpr int kAxisLat = 0;   // IMU axis used for lateral G (left/right).
constexpr int kAxisLong = 1;  // IMU axis used for longitudinal G (accel/brake).
constexpr int kAxisVert = 2;  // IMU axis used for vertical (gravity) component.
constexpr float kAxisLatSign = 1.0f;
constexpr float kAxisLongSign = 1.0f;
constexpr float kAxisVertSign = 1.0f;

struct GForceRefs {
  lv_obj_t *root;
  lv_obj_t *target;
  lv_obj_t *ball;
  lv_obj_t *labelTop;
  lv_obj_t *labelBottom;
  lv_obj_t *labelLeft;
  lv_obj_t *labelRight;
  lv_obj_t *labelMax;
  lv_obj_t *resetBtn;
  lv_obj_t *resetLabel;
};

struct GForceState {
  float gravity[3];
  float filtered[2];
  float peakForward;
  float peakBrake;
  float peakLeft;
  float peakRight;
  bool calibrating;
  uint32_t calibrationStartMs;
  float calibrationSum[3];
  uint16_t calibrationSamples;
};

GForceRefs refs{};
GForceState state{};
lv_timer_t *updateTimer = nullptr;

void reset_peaks() {
  state.peakForward = 0.0f;
  state.peakBrake = 0.0f;
  state.peakLeft = 0.0f;
  state.peakRight = 0.0f;
  state.filtered[0] = 0.0f;
  state.filtered[1] = 0.0f;
  if (!refs.root) return;
  int32_t centerX = lv_obj_get_width(refs.root) / 2;
  int32_t centerY = lv_obj_get_height(refs.root) / 2;
  lv_obj_set_pos(refs.ball, centerX - (kBallSize / 2), centerY - (kBallSize / 2));
}

void start_calibration() {
  state.calibrating = true;
  state.calibrationStartMs = millis();
  state.calibrationSum[0] = 0.0f;
  state.calibrationSum[1] = 0.0f;
  state.calibrationSum[2] = 0.0f;
  state.calibrationSamples = 0;
  state.gravity[0] = 0.0f;
  state.gravity[1] = 0.0f;
  state.gravity[2] = 0.0f;
  reset_peaks();
}

float clampf(float v, float minv, float maxv) {
  if (v < minv) return minv;
  if (v > maxv) return maxv;
  return v;
}

void update_ball(float gx, float gy) {
  if (!refs.root) return;
  int32_t centerX = lv_obj_get_width(refs.root) / 2;
  int32_t centerY = lv_obj_get_height(refs.root) / 2;
  int32_t maxRadius = (kCircleSize / 2) - (kBallSize / 2) - 4;

  float x = gx * maxRadius;
  float y = -gy * maxRadius;
  float r = sqrtf(x * x + y * y);
  if (r > (float)maxRadius && r > 0.0f) {
    float scale = (float)maxRadius / r;
    x *= scale;
    y *= scale;
  }

  int32_t posX = centerX + (int32_t)lroundf(x) - (kBallSize / 2);
  int32_t posY = centerY + (int32_t)lroundf(y) - (kBallSize / 2);

  lv_obj_set_pos(refs.ball, posX, posY);
  lv_obj_invalidate(refs.ball);
}

void update_labels(float forward, float brake, float left, float right) {
  char buf[16];

  snprintf(buf, sizeof(buf), "%.2f", (double)forward);
  lv_label_set_text(refs.labelTop, buf);

  snprintf(buf, sizeof(buf), "%.2f", (double)brake);
  lv_label_set_text(refs.labelBottom, buf);

  snprintf(buf, sizeof(buf), "%.2f", (double)left);
  lv_label_set_text(refs.labelLeft, buf);

  snprintf(buf, sizeof(buf), "%.2f", (double)right);
  lv_label_set_text(refs.labelRight, buf);
}

void update_peaks(float gLat, float gLong) {
  if (gLong > state.peakForward + kPeakEpsilon) {
    state.peakForward = gLong;
  }
  if (-gLong > state.peakBrake + kPeakEpsilon) {
    state.peakBrake = -gLong;
  }
  if (-gLat > state.peakLeft + kPeakEpsilon) {
    state.peakLeft = -gLat;
  }
  if (gLat > state.peakRight + kPeakEpsilon) {
    state.peakRight = gLat;
  }
}

void reset_btn_event(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    reset_peaks();
  }
}

void update_timer_cb(lv_timer_t *timer) {
  LV_UNUSED(timer);
  if (screen_nav_is_transitioning()) {
    return;
  }

  float acc[3] = {0.0f, 0.0f, 0.0f};
  if (!imu_qmi8658_read_accel(acc[0], acc[1], acc[2])) {
    return;
  }

  float ax = acc[kAxisLat] * kAxisLatSign;
  float ay = acc[kAxisLong] * kAxisLongSign;
  float az = acc[kAxisVert] * kAxisVertSign;

  uint32_t now = millis();
  if (state.calibrating) {
    state.calibrationSum[0] += ax;
    state.calibrationSum[1] += ay;
    state.calibrationSum[2] += az;
    state.calibrationSamples++;
    if ((uint32_t)(now - state.calibrationStartMs) >= kCalibrationMs &&
        state.calibrationSamples > 0) {
      state.gravity[0] = state.calibrationSum[0] / state.calibrationSamples;
      state.gravity[1] = state.calibrationSum[1] / state.calibrationSamples;
      state.gravity[2] = state.calibrationSum[2] / state.calibrationSamples;
      state.calibrating = false;
    } else {
      update_labels(0.0f, 0.0f, 0.0f, 0.0f);
      update_ball(0.0f, 0.0f);
      return;
    }
  }

  state.gravity[0] = (1.0f - kGravityAlpha) * state.gravity[0] + kGravityAlpha * ax;
  state.gravity[1] = (1.0f - kGravityAlpha) * state.gravity[1] + kGravityAlpha * ay;
  state.gravity[2] = (1.0f - kGravityAlpha) * state.gravity[2] + kGravityAlpha * az;

  float linX = ax - state.gravity[0];
  float linY = ay - state.gravity[1];

  float gLat = linX / kGravityMs2;
  float gLong = linY / kGravityMs2;

  float alphaLat = (fabsf(gLat) < kStillThreshold) ? kSmoothingAlphaStill : kSmoothingAlpha;
  float alphaLong = (fabsf(gLong) < kStillThreshold) ? kSmoothingAlphaStill : kSmoothingAlpha;
  state.filtered[0] = state.filtered[0] + alphaLat * (gLat - state.filtered[0]);
  state.filtered[1] = state.filtered[1] + alphaLong * (gLong - state.filtered[1]);

  float dispLat = clampf(state.filtered[0], -2.0f, 2.0f);
  float dispLong = clampf(state.filtered[1], -2.0f, 2.0f);

  update_ball(dispLat, dispLong);
  update_peaks(gLat, gLong);
  update_labels(state.peakForward, state.peakBrake, state.peakLeft, state.peakRight);
}

lv_obj_t *create_tick(lv_obj_t *parent, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t width) {
  lv_point_t *points = static_cast<lv_point_t *>(lv_mem_alloc(sizeof(lv_point_t) * 2));
  if (!points) return nullptr;
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;

  lv_obj_t *line = lv_line_create(parent);
  lv_line_set_points(line, points, 2);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_color(line, lv_color_hex(0xcfd6df), 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  return line;
}

void build_ticks(lv_obj_t *parent) {
  int32_t centerX = lv_obj_get_width(parent) / 2;
  int32_t centerY = lv_obj_get_height(parent) / 2;
  int32_t radius = kCircleSize / 2;

  create_tick(parent, centerX - kTickLength, centerY, centerX + kTickLength, centerY, 2);
  create_tick(parent, centerX, centerY - kTickLength, centerX, centerY + kTickLength, 2);

  for (int i = 1; i <= 2; ++i) {
    int32_t offset = (radius / 3) * i;
    create_tick(parent, centerX - kSmallTickLength, centerY - offset,
                centerX + kSmallTickLength, centerY - offset, 2);
    create_tick(parent, centerX - kSmallTickLength, centerY + offset,
                centerX + kSmallTickLength, centerY + offset, 2);
    create_tick(parent, centerX - offset, centerY - kSmallTickLength,
                centerX - offset, centerY + kSmallTickLength, 2);
    create_tick(parent, centerX + offset, centerY - kSmallTickLength,
                centerX + offset, centerY + kSmallTickLength, 2);
  }
}

void build_gforce_screen(lv_obj_t *root) {
  refs.root = root;
  lv_obj_set_style_bg_color(refs.root, lv_color_hex(0x0b0f14), 0);
  lv_obj_set_style_bg_opa(refs.root, LV_OPA_COVER, 0);
  lv_obj_clear_flag(refs.root, LV_OBJ_FLAG_SCROLLABLE);

  refs.target = lv_obj_create(refs.root);
  lv_obj_set_size(refs.target, kCircleSize, kCircleSize);
  lv_obj_set_style_radius(refs.target, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(refs.target, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(refs.target, 2, 0);
  lv_obj_set_style_border_color(refs.target, lv_color_hex(0xdde3ec), 0);
  lv_obj_clear_flag(refs.target, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(refs.target);

  build_ticks(refs.root);

  refs.ball = lv_obj_create(refs.root);
  lv_obj_set_size(refs.ball, kBallSize, kBallSize);
  lv_obj_set_style_radius(refs.ball, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(refs.ball, lv_color_hex(0xff5a3d), 0);
  lv_obj_set_style_bg_opa(refs.ball, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.ball, 0, 0);
  lv_obj_clear_flag(refs.ball, LV_OBJ_FLAG_CLICKABLE);

  refs.labelTop = lv_label_create(refs.root);
  lv_obj_set_style_text_font(refs.labelTop, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(refs.labelTop, lv_color_hex(0xf5f8ff), 0);
  lv_obj_align(refs.labelTop, LV_ALIGN_TOP_MID, 0, 18);

  refs.labelBottom = lv_label_create(refs.root);
  lv_obj_set_style_text_font(refs.labelBottom, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(refs.labelBottom, lv_color_hex(0xf5f8ff), 0);
  lv_obj_align(refs.labelBottom, LV_ALIGN_BOTTOM_MID, 0, -18);

  refs.labelLeft = lv_label_create(refs.root);
  lv_obj_set_style_text_font(refs.labelLeft, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(refs.labelLeft, lv_color_hex(0xf5f8ff), 0);
  lv_obj_align(refs.labelLeft, LV_ALIGN_LEFT_MID, 10, -10);

  refs.labelRight = lv_label_create(refs.root);
  lv_obj_set_style_text_font(refs.labelRight, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(refs.labelRight, lv_color_hex(0xf5f8ff), 0);
  lv_obj_align(refs.labelRight, LV_ALIGN_RIGHT_MID, -10, -10);

  refs.labelMax = lv_label_create(refs.root);
  lv_label_set_text(refs.labelMax, "Max");
  lv_obj_set_style_text_font(refs.labelMax, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refs.labelMax, lv_color_hex(0x9aa7b7), 0);
  lv_obj_align(refs.labelMax, LV_ALIGN_BOTTOM_LEFT, 18, -24);

  refs.resetBtn = lv_btn_create(refs.root);
  lv_obj_set_size(refs.resetBtn, 64, 64);
  lv_obj_set_style_radius(refs.resetBtn, 32, 0);
  lv_obj_set_style_bg_color(refs.resetBtn, lv_color_hex(0x1c2633), 0);
  lv_obj_set_style_bg_opa(refs.resetBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(refs.resetBtn, 2, 0);
  lv_obj_set_style_border_color(refs.resetBtn, lv_color_hex(0x2d3a4b), 0);
  lv_obj_align(refs.resetBtn, LV_ALIGN_BOTTOM_RIGHT, -20, -18);
  lv_obj_add_event_cb(refs.resetBtn, reset_btn_event, LV_EVENT_CLICKED, nullptr);

  refs.resetLabel = lv_label_create(refs.resetBtn);
  lv_label_set_text(refs.resetLabel, LV_SYMBOL_REFRESH);
  lv_obj_set_style_text_font(refs.resetLabel, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(refs.resetLabel, lv_color_hex(0xf5f8ff), 0);
  lv_obj_center(refs.resetLabel);

  updateTimer = lv_timer_create(update_timer_cb, kUpdateMs, nullptr);

  start_calibration();
  update_labels(0.0f, 0.0f, 0.0f, 0.0f);
  update_ball(0.0f, 0.0f);
}
}  // namespace

void screen_gforce_init(void) {
  // Initialized when attached to a parent or created on demand.
}

lv_obj_t *screen_gforce_get_screen(void) {
  if (gforceScreen) return gforceScreen;
  gforceScreen = lv_obj_create(nullptr);
  build_gforce_screen(gforceScreen);
  return gforceScreen;
}

void screen_gforce_attach(lv_obj_t *parent) {
  if (!parent || gforceScreen) return;
  gforceScreen = parent;
  build_gforce_screen(gforceScreen);
}
