# LVGL AMOLED 1.64 Demo (RP2350)

This example demonstrates a **known-good LVGL configuration** for the Waveshare 1.64" AMOLED display (QSPI, CO5300 driver) running on RP2350.

It exists as a **smoke test and reference implementation** to prevent regressions when integrating LVGL into the main lap timer firmware.

If this demo works, LVGL *will* work in the lap timer.  
If this demo breaks, **do not proceed** with UI work until it is fixed.

---

## What This Demo Proves

- LVGL initializes correctly (no black screen)
- Packed LVGL draw buffers are flushed correctly over QSPI + DMA
- RGB565 color order is correct
- No “snow” or corrupted output
- Display tearing is mitigated **without a TE pin**

---

## Hardware / Software Assumptions

- **Display:** Waveshare AMOLED 1.64"
- **Resolution:** 280 × 456
- **Interface:** QSPI (PIO + DMA)
- **Driver IC:** CO5300
- **MCU:** RP2350
- **LVGL:** v8.4.x
- **Arduino Core:** RP2350 Arduino core with PIO/DMA enabled

---

## Expected Output

When flashed, the demo should show:

- Solid **red** background
- Solid **green** square moving left/right
- White label at the top
- **No black screen**
- **No snow/static**
- **No tearing or trails**

If you see artifacts, consult the **Tuning** section below.

---

## Why This Demo Exists

The Waveshare driver function:
AMOLED_1IN64_DisplayWindows()


**assumes a full-screen framebuffer**.  
LVGL does **not** provide a full framebuffer — it provides **packed partial buffers**.

Passing LVGL buffers directly into `DisplayWindows()` causes:
- out-of-bounds DMA reads
- static/snow
- black screen
- inconsistent behavior

This demo uses a **custom packed DMA flush** instead.

---

## Key Implementation Details

### Packed LVGL Flush (Required)

The LVGL flush callback:

1. Receives LVGL’s packed RGB565 buffer
2. Optionally byte-swaps into a temporary buffer
3. Sets the display window (`SetWindows`)
4. Issues `RAMWR (0x2C)`
5. Streams pixel data via **QSPI DMA**
6. Adds a small pacing delay to reduce tearing

**Do NOT use** `AMOLED_1IN64_DisplayWindows()` for LVGL.

---

## Tearing Mitigation (No TE Pin)

This display does **not expose a TE/VSYNC pin**, so tearing is mitigated in software:

- Large LVGL draw buffer:
  ```cpp
  static const int BUF_LINES = 120;


Small post-flush delay:

DEV_Delay_ms(1);


Forced screen invalidation:

lv_obj_invalidate(lv_scr_act());


Slightly slower animations (≥1200 ms)

This combination was empirically verified to eliminate visible tearing.

LVGL Configuration (lv_conf.h)
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_COLOR_SCREEN_TRANSP 0
#define LV_DISP_DEF_REFR_PERIOD 10


Color swapping is handled in the flush callback, not by LVGL.

File Layout
lvgl_amoled_1in64/
├── lvgl_amoled_1in64.ino
├── lv_conf.h
└── README.md   ← this file

How to Use This Demo

Open lvgl_amoled_1in64.ino

Verify lv_conf.h is in the same folder

Select the correct RP2350 board

Build and flash

Confirm expected output

Only proceed to lap timer UI integration after this demo is stable.

Common Failure Modes
Symptom	Likely Cause
Black screen	Using DisplayWindows() with LVGL buffer
Snow/static	Out-of-bounds DMA reads
Wrong colors	Missing byte swap
Trails/tearing	No pacing delay or no invalidation
Flicker	Draw buffer too small
DO / DON’T Rules
DO

Treat this demo as the baseline

Use packed DMA flush

Keep byte swap in flush

Keep pacing delay

Increase buffer size before changing logic

DON’T

Call AMOLED_1IN64_DisplayWindows() with LVGL buffers

Assume LVGL provides a full framebuffer

Remove delay without testing

Mix LVGL color swap with driver swap

When This Demo Passes

You may:

Integrate LVGL into the lap timer UI

Add widgets, pages, animations

Add touch input (FT3168 → LVGL indev)

If display behavior regresses, return here first.
