#include <Arduino.h>

#include "DEV_Config.h"
#include "AMOLED_1in64.h"

#define LV_CONF_INCLUDE_SIMPLE 1
#include "lv_conf.h"
#include <lvgl.h>

static_assert(sizeof(lv_color_t) == 2, "LVGL must be 16-bit RGB565");

// ========= SETTINGS =========
#define SWAP_BYTES_IN_FLUSH   1       // keep as your working value
static const int BUF_LINES   = 120;   // your working improvement
static const int POST_FLUSH_DELAY_MS = 1; // try 1, then 2 if needed

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lv_buf1[AMOLED_1IN64_WIDTH * BUF_LINES];
static uint16_t tmp565[AMOLED_1IN64_WIDTH * BUF_LINES];

static volatile uint32_t flush_count = 0;

static inline uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

/**
 * Send a packed RGB565 tile buffer to the display for region [x1..x2, y1..y2].
 * The incoming `src` is contiguous (row-major) with width = (x2-x1+1).
 * We send in stripes of up to BUF_LINES.
 */
static void blit_packed_region(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const lv_color_t* src)
{
  const uint32_t w = (x2 - x1 + 1);
  const uint32_t h = (y2 - y1 + 1);
  if (w == 0 || h == 0) return;
  if (w > AMOLED_1IN64_WIDTH) return;

  uint32_t rows_sent = 0;

  while (rows_sent < h) {
    const uint32_t stripe_rows = (h - rows_sent > (uint32_t)BUF_LINES) ? (uint32_t)BUF_LINES : (h - rows_sent);
    const uint32_t px = w * stripe_rows;

    const uint16_t* s16 = (const uint16_t*)src + (rows_sent * w);

#if SWAP_BYTES_IN_FLUSH
    for (uint32_t i = 0; i < px; i++) tmp565[i] = bswap16(s16[i]);
#else
    memcpy(tmp565, s16, px * 2);
#endif

    const uint32_t ys = y1 + rows_sent;
    const uint32_t ye = ys + stripe_rows - 1;

    // Window set (driver expects end-exclusive)
    QSPI_1Wrie_Mode(&qspi);
    AMOLED_1IN64_SetWindows(x1, ys, x2 + 1, ye + 1);

    // RAMWR then stream pixels
    QSPI_Select(qspi);
    QSPI_Pixel_Write(qspi, 0x2c);

    QSPI_4Wrie_Mode(&qspi);
    channel_config_set_dreq(&c, pio_get_dreq(qspi.pio, qspi.sm, true));

    __asm__ volatile("dmb");
    dma_channel_configure(
      dma_tx,
      &c,
      &qspi.pio->txf[qspi.sm],
      (const uint8_t*)tmp565,
      px * 2,
      true
    );
    while (dma_channel_is_busy(dma_tx)) {}
    __asm__ volatile("dmb");

    QSPI_Deselect(qspi);

    // === Key addition: tiny pacing delay to reduce tearing ===
    if (POST_FLUSH_DELAY_MS > 0) {
      DEV_Delay_ms(POST_FLUSH_DELAY_MS);
    }

    rows_sent += stripe_rows;
  }
}

static void disp_flush_cb(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p)
{
  flush_count++;

  // Optional light logging
  if (flush_count <= 5 || (flush_count % 100) == 0) {
    Serial.print("[FLUSH] #"); Serial.print(flush_count);
    Serial.print(" "); Serial.print(area->x1); Serial.print(",");
    Serial.print(area->y1); Serial.print(" -> ");
    Serial.print(area->x2); Serial.print(",");
    Serial.println(area->y2);
  }

  blit_packed_region(area->x1, area->y1, area->x2, area->y2, color_p);
  lv_disp_flush_ready(disp);
}

static void build_ui()
{
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  lv_obj_t *box = lv_obj_create(lv_scr_act());
  lv_obj_set_size(box, 90, 90);
  lv_obj_set_style_bg_color(box, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_outline_width(box, 0, 0);
  lv_obj_set_style_shadow_width(box, 0, 0);
  lv_obj_set_style_radius(box, 0, 0);
  lv_obj_align(box, LV_ALIGN_CENTER, -120, 0);

  static lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, box);
  lv_anim_set_values(&a, -120, 120);
  lv_anim_set_time(&a, 1200);
  lv_anim_set_playback_time(&a, 1200);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
  lv_anim_start(&a);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "LVGL RGB TEST");
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 12);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=== LVGL PACKED DMA + PACING DELAY ===");
  Serial.print("BUF_LINES="); Serial.println(BUF_LINES);
  Serial.print("POST_FLUSH_DELAY_MS="); Serial.println(POST_FLUSH_DELAY_MS);
  Serial.print("SWAP_BYTES_IN_FLUSH="); Serial.println(SWAP_BYTES_IN_FLUSH);

  DEV_Module_Init();
  AMOLED_1IN64_Init();
  AMOLED_1IN64_SetBrightness(140);
  AMOLED_1IN64_Clear(BLACK);

  lv_init();

  lv_disp_draw_buf_init(&draw_buf, lv_buf1, NULL, AMOLED_1IN64_WIDTH * BUF_LINES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = AMOLED_1IN64_WIDTH;
  disp_drv.ver_res = AMOLED_1IN64_HEIGHT;
  disp_drv.flush_cb = disp_flush_cb;
  disp_drv.draw_buf = &draw_buf;

  lv_disp_drv_register(&disp_drv);

  build_ui();
  Serial.println("setup done");
}

void loop()
{
  // Force full invalidation (your “fix”)
  lv_obj_invalidate(lv_scr_act());

  static uint32_t last = millis();
  uint32_t now = millis();
  lv_tick_inc(now - last);
  last = now;

  lv_timer_handler();

  static uint32_t last_hb = 0;
  if (millis() - last_hb > 1000) {
    last_hb = millis();
    Serial.print("[HB] flush_count="); Serial.println(flush_count);
  }

  DEV_Delay_ms(5);
}
