# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]
### Added
- IR Beam Test tile with live edge rate, qualified detection, signal statistics, resettable counters, and strength-coded audible feedback.
- Parametric receiver and beacon optical baffles plus an outdoor beam-width test procedure.
- SD card session logging for lap and reaction events, including per-session summaries under `/PILAPTIMER/SESSIONS`.

### Changed
- Production IR triggering now uses the documented 20 ms edge-count detector with hysteresis and the locked cooldown/minimum-lap safeguards.
- G-force monitor tile response smoothing and axis orientation mapping.

## [0.2.0] - 2026-01-29

### Added
- Distinct lap beep sequences for best laps versus normal laps without blocking timer updates.
- IR lap gating with edge detection and a release window to prevent continuous-beam retriggers.

### Changed
- Settings screen layout with larger labels/controls, inline +/- buttons, and stable sizing.
- Main screen timing to show total session time and larger right-aligned lap info.
- Reset interaction to trigger on tap (no hold requirement).

### Fixed
- Removed spinbox cursor/selection highlight and cleaned value background styling.
- Adjusted the BEST lap checkmark position to avoid overlapping the lap label.
