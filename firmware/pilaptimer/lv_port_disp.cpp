#include "lv_port_disp.h"

#include "DEV_Config.h"
#include "AMOLED_1in64.h"
#include "qspi_pio.h"

static lv_disp_draw_buf_t s_draw_buf;

static constexpr uint32_t kBufLines = 40;
static constexpr uint32_t kPostFlushDelayMs = 1;
static constexpr bool kSwapBytesInFlush = false;

static_assert(sizeof(lv_color_t) == 2, "LVGL must be configured for RGB565");

static lv_color_t s_buf1[LVGL_LOGICAL_W * kBufLines];
static uint16_t s_tmp565[LVGL_LOGICAL_W * kBufLines];

static inline uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

static void lv_port_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  const int32_t logical_width = area->x2 - area->x1 + 1;
  const int32_t logical_height = area->y2 - area->y1 + 1;

  if (logical_width <= 0 || logical_height <= 0) {
    lv_disp_flush_ready(disp_drv);
    return;
  }

  const int32_t phys_x_start = area->y1;
  const int32_t phys_x_end = area->y2;
  const int32_t phys_y_start = (int32_t)LVGL_LOGICAL_W - 1 - area->x2;
  const int32_t phys_y_end = (int32_t)LVGL_LOGICAL_W - 1 - area->x1;

  const int32_t phys_width = phys_x_end - phys_x_start + 1;
  const int32_t phys_height = phys_y_end - phys_y_start + 1;

  int32_t rows_sent = 0;
  while (rows_sent < phys_height) {
    const int32_t stripe_rows = (phys_height - rows_sent > (int32_t)kBufLines)
                                    ? (int32_t)kBufLines
                                    : (phys_height - rows_sent);
    const int32_t pixels = phys_width * stripe_rows;

    for (int32_t row = 0; row < stripe_rows; ++row) {
      const int32_t phys_y = phys_y_start + rows_sent + row;
      const int32_t logical_x = (int32_t)LVGL_LOGICAL_W - 1 - phys_y;
      for (int32_t col = 0; col < phys_width; ++col) {
        const int32_t phys_x = phys_x_start + col;
        const int32_t logical_y = phys_x;
        const int32_t src_index =
            (logical_y - area->y1) * logical_width + (logical_x - area->x1);
        const uint16_t src_px = ((uint16_t *)color_p)[src_index];
        s_tmp565[row * phys_width + col] = kSwapBytesInFlush ? bswap16(src_px) : src_px;
      }
    }

    QSPI_1Wrie_Mode(&qspi);
    // NOTE: DisplayWindows() assumes a full 280x456 framebuffer and will read out of bounds
    // when given LVGL's packed tile buffers. Use SetWindows() + RAMWR instead.
    AMOLED_1IN64_SetWindows((uint32_t)phys_x_start,
                            (uint32_t)(phys_y_start + rows_sent),
                            (uint32_t)phys_x_end + 1,
                            (uint32_t)(phys_y_start + rows_sent + stripe_rows));
    QSPI_Select(qspi);
    QSPI_Pixel_Write(qspi, 0x2c);

    QSPI_4Wrie_Mode(&qspi);
    channel_config_set_dreq(&c, pio_get_dreq(qspi.pio, qspi.sm, true));
    __asm__ volatile("dmb");
    dma_channel_configure(
        dma_tx,
        &c,
        &qspi.pio->txf[qspi.sm],
        (uint8_t *)s_tmp565,
        pixels * 2,
        true);

    while (dma_channel_is_busy(dma_tx)) {
    }
    __asm__ volatile("dmb");

    QSPI_Deselect(qspi);
    if (kPostFlushDelayMs > 0) {
      DEV_Delay_ms(kPostFlushDelayMs);
    }

    rows_sent += stripe_rows;
  }

  lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init() {
  lv_disp_draw_buf_init(&s_draw_buf, s_buf1, nullptr, LVGL_LOGICAL_W * kBufLines);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LVGL_LOGICAL_W;
  disp_drv.ver_res = LVGL_LOGICAL_H;
  disp_drv.flush_cb = lv_port_disp_flush;
  disp_drv.draw_buf = &s_draw_buf;

  lv_disp_drv_register(&disp_drv);
}
