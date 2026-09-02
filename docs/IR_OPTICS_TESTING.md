# IR beam shaping and outdoor test procedure

The receiver detects a 38 kHz carrier. Software can reject weak or incomplete
packets, but it cannot create a sharp start line from a wide optical field of
view. The most repeatable result comes from using the validated detector in the
firmware together with black mechanical baffles on both ends.

## Target geometry

The important axis is along the direction of travel. Keep the beam narrow on
that horizontal axis, while leaving more vertical tolerance for chassis pitch,
bumps, and mounting-height error.

A rectangular tunnel or slit has an approximate full acceptance angle:

```text
full angle = 2 * atan(clear opening / (2 * tunnel depth))
```

Starting dimensions:

| Part | Tunnel depth | Clear horizontal opening | Clear vertical opening | Approx. horizontal angle |
|---|---:|---:|---:|---:|
| Receiver hood | 35 mm | 4 mm | 10 mm | 6.5 degrees |
| Beacon hood | 40 mm | 3 mm | 18 mm | 4.3 degrees |

At 15 ft, a 4.3 degree beacon angle is about 13.5 in wide before receiver field
of view, reflections, and alignment error are included. Print alternate beacon
inserts with 2, 3, and 4 mm openings and begin with 3 mm.

## Case changes

- Recess the receiver behind a straight matte-black tunnel. Put the sensor's
  optical centre at the closed end of the tunnel.
- Use a tall vertical slot. Narrowing only the horizontal opening preserves
  useful vertical tolerance.
- Add one internal knife-edge baffle halfway down the tunnel if the printed
  walls are glossy.
- Print the hood in opaque black material. Paint the inside with flat black
  paint if necessary. Avoid translucent filament and shiny bare PETG inside.
- Do not place a curved or frosted window over the opening. It will scatter IR
  and widen the field of view. If weather protection is required, use a flat,
  clear IR-compatible window directly over the front opening and retest it.
- Recess each beacon LED behind the beacon hood. If several LEDs are spread
  horizontally, give each LED its own tunnel or arrange them vertically.
- Make the final 10 to 15 mm of the beacon hood a removable insert. This makes
  it practical to compare slit widths without reprinting the full enclosure.
- Keep the receiver wiring from the known-good configuration: 3.3 V, GPIO1,
  `INPUT_PULLUP`, common ground, and no divider. A 0.1 uF ceramic capacitor at
  the receiver remains recommended.

A parametric printable starting point is provided in
`hardware/ir-optics/beam_baffles.scad`. Its flange and opening dimensions must
be checked against the actual case, receiver package, and LED layout before
printing.

## Using the IR Beam Test screen

1. Test outdoors in the same sunlight and at the same 10 to 15 ft separation
   expected on the track.
2. Open **IR Beam Test** by swiping left past G-Force. Audio starts enabled.
3. Fix the beacon in its race position. Move the receiver through the start-line
   area at its normal mounting height and orientation.
4. Mark the first audible detection and the last audible detection on the
   ground. The distance between them is the effective trigger-zone width.
5. Repeat the pass in both directions and at the near and far sides of the
   track. A useful design produces similar boundaries in every pass.
6. Use **PEAK** to confirm adequate signal margin near the centre. Use **HIT %**
   to find unstable edges. A boundary that chatters between detected and quiet
   needs more optical margin or a cleaner baffle, not a lower software
   threshold.
7. Press **RESET STATS** before each configuration. Record hood depth, slit
   width, range, sunlight direction, ground width, peak edges, and hit rate.
8. Repeat with the beacon powered off. There should be no qualified hits. If
   there are, shield reflections and sunlight before changing the validated
   detector thresholds.

Do not tune only while stationary. Finish with several driven crossings,
including the fastest expected speed, before accepting a baffle combination.
