# LVGL AMOLED 1.64 Demo (RP2350)

This folder documents the LVGL smoke-test demo for the Waveshare RP2350 Touch AMOLED 1.64" board.

## What this demo verifies

- The LVGL flush pipeline uses packed RGB565 data.
- The display driver receives byte-swapped RGB565 in the flush callback.
- The QSPI DMA transfer path runs reliably without visible tearing.

## Expected behavior

- Solid red background.
- A solid green square animates smoothly.
- The demo runs continuously without corruption or lockups.

## Related documentation

- Display pipeline reference: `docs/display_lvgl_waveshare_1in64.md`
