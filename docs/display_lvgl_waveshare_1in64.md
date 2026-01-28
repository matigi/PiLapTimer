# LVGL on Waveshare AMOLED 1.64 (RP2350) — Working Reference

This document captures the **known-good LVGL display pipeline** for the Waveshare 1.64" AMOLED module (QSPI, CO5300 driver) on RP2350.

It exists to prevent regressions after extensive debugging and experimentation.

If you are integrating LVGL into this project, **follow this document exactly** before making changes.

---

## Hardware & Software Context

- **Display:** Waveshare AMOLED 1.64"
- **Resolution:** 280 × 456
- **Interface:** QSPI (PIO + DMA)
- **Display Driver IC:** CO5300
- **MCU:** RP2350
- **LVGL Version:** 8.4.x
- **Arduino Core:** RP2350 Arduino core (PIO + DMA enabled)

---

## Summary: What Works (Golden Path)

### Display Pipeline (Required)
- LVGL **must use a packed flush path**
- Pixel data from LVGL **cannot** be passed directly to `AMOLED_1IN64_DisplayWindows()`
- Correct approach:
  1. `AMOLED_1IN64_SetWindows(...)`
  2. Send `RAMWR (0x2C)`
  3. Stream **packed RGB565 data via QSPI DMA**

### Color Handling
- LVGL runs in **RGB565**
- Waveshare driver expects **byte-swapped RGB565**
- Solution:
  - Keep `LV_COLOR_16_SWAP = 0`
  - Perform byte swap **inside the flush callback**

### Tearing Mitigation (No TE Pin Available)
Because the Waveshare board does **not expose a TE/VSYNC pin**, tearing must be mitigated in software:

- Increase LVGL draw buffer height:
  ```cpp
  static const int BUF_LINES = 120;
Add a post-flush pacing delay:

DEV_Delay_ms(1);
Force a full invalidation at a controlled rate:

lv_obj_invalidate(lv_scr_act());
Slow animations slightly (example: 1200 ms instead of 900 ms)

This combination eliminates visible tearing.

Required LVGL Configuration (lv_conf.h)
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_COLOR_SCREEN_TRANSP 0

// Works reliably with tearing mitigation in place
#define LV_DISP_DEF_REFR_PERIOD 10
Why Earlier Approaches Failed
Using AMOLED_1IN64_DisplayWindows() with LVGL buffers
That function assumes the image pointer refers to a full-screen framebuffer and computes offsets like:

pixel_offset = (row * AMOLED_1IN64.WIDTH + Xstart) * 2;
LVGL provides packed, partial buffers, which results in:

DMA reading past valid memory

Visual “snow” / static

Corrupted pixels

Line-by-line DisplayWindows() calls
Repeated window resets + partial DMA transfers break the QSPI transaction flow and cause:

Black screen

Corrupted output

Lockups

Relying on LV_COLOR_16_SWAP
The Waveshare driver already makes byte-order assumptions. Mixing swap responsibility between LVGL and the driver causes inconsistent colors.

Working Display Strategy (Implementation Notes)
Flush Callback Responsibilities
The LVGL flush callback must:

Receive packed LVGL tile buffer

Optionally byte-swap into a temporary buffer

Send pixels using one contiguous DMA stream per stripe

Add a short delay after each DMA transfer

Pseudo-flow:

LVGL flush →
  packed buffer →
    byte swap →
      QSPI SetWindow →
        RAMWR →
          DMA stream →
            short delay →
              done
Known-Good Runtime Parameters
Parameter	Value
BUF_LINES	120
POST_FLUSH_DELAY_MS	1
Animation duration	≥ 1200 ms
Screen invalidation	Yes
LVGL refresh	10 ms
Do / Don’t Rules
DO
Use packed DMA flush

Swap bytes in flush callback

Add pacing delay after DMA

Invalidate screen at fixed cadence if tearing appears

Treat this setup as a baseline

DON’T
Call AMOLED_1IN64_DisplayWindows() with LVGL buffers

Assume LVGL buffers are full-screen

Remove the delay without re-testing tearing

Mix LVGL color swap with driver swap

Reference Implementation
See:

firmware/examples/lvgl_amoled_1in64/
This example:

Displays a solid red background

Animates a solid green square

Runs indefinitely without tearing

Serves as the smoke test before UI changes

Future Improvements
Replace full-screen invalidation with region-based invalidation (union of old + new bounds)

Explore lowering QSPI clock if power budget allows

Enable TE sync if future hardware revision exposes the pin

