#include "lv_port_disp.h"

#include "AMOLED_1in64.h"
#include "DEV_Config.h"
#include "qspi_pio.h"

static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t s_buf1[LVGL_LOGICAL_W * 40];

static void lv_port_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;

  const int32_t phys_x_start = area->y1;
  const int32_t phys_x_end = area->y2 + 1;
  const int32_t phys_y_start = (int32_t)LVGL_LOGICAL_W - 1 - area->x2;
  const int32_t phys_y_end = (int32_t)LVGL_LOGICAL_W - area->x1;

  static lv_color_t line_buf[LVGL_LOGICAL_H];

  for (int32_t phys_y = phys_y_start; phys_y < phys_y_end; ++phys_y) {
    const int32_t logical_x = (int32_t)LVGL_LOGICAL_W - 1 - phys_y;

    for (int32_t i = 0; i < height; ++i) {
      const int32_t logical_y = area->y1 + i;
      const int32_t src_index = (logical_y - area->y1) * width + (logical_x - area->x1);
      line_buf[i] = color_p[src_index];
    }

    QSPI_1Wrie_Mode(&qspi);
    AMOLED_1IN64_SetWindows((uint32_t)phys_x_start, (uint32_t)phys_y,
                            (uint32_t)phys_x_end, (uint32_t)phys_y + 1);
    QSPI_Select(qspi);
    QSPI_Pixel_Write(qspi, 0x2c);

    QSPI_4Wrie_Mode(&qspi);
    channel_config_set_dreq(&c, pio_get_dreq(qspi.pio, qspi.sm, true));
    dma_channel_configure(
        dma_tx,
        &c,
        &qspi.pio->txf[qspi.sm],
        (uint8_t *)line_buf,
        (phys_x_end - phys_x_start) * 2,
        true);

    while (dma_channel_is_busy(dma_tx)) {
    }

    QSPI_Deselect(qspi);
  }

  lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init() {
  lv_disp_draw_buf_init(&s_draw_buf, s_buf1, nullptr, LVGL_LOGICAL_W * 40);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LVGL_LOGICAL_W;
  disp_drv.ver_res = LVGL_LOGICAL_H;
  disp_drv.flush_cb = lv_port_disp_flush;
  disp_drv.draw_buf = &s_draw_buf;

  lv_disp_drv_register(&disp_drv);
}
