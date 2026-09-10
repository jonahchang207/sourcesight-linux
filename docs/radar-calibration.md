# Radar calibration — September 10, 2026

Measured from live CS2 screenshots while the local player was alive on Dust II.
The game window was at desktop (13, 43), with a 1894 × 1024 client area.
Radar coordinates below are relative to that client area, not the desktop.

- Radar top-left: (24, 24).
- Radar diameter: 237 pixels; center: (142.5, 142.5).
- Reference viewport height: 1024 pixels.
- HUD scaling and radar HUD size: 1.0.
- Reference zoom: 0.70; calibrated world radius: 1260 units.
- Fine scale correction: 1.0; extra offset: (0, 0).

The previous 199 × 200 rectangle at (44, 50) placed the center roughly eight
pixels too low. Visually corresponding teammate markers indicated roughly
1.22 times the previous marker separation. Increasing the rectangle size and
adjusting the reference range produced close overlap in two live screenshots
after restarting the overlay. This is visual calibration, not an exact
measurement of all players or a derivation from CS2 overview metadata.

Applied to build/configs/legit.json, with the previous profile retained as
legit.before-radar-calibration.json.bak. Only radar settings were changed.
The saved calibration_height makes rectangle position, dimensions, offsets,
and projection padding scale uniformly with the actual overlay viewport height.
Zero calibration_height retains legacy pixel coordinates. Calibrations assume
the same HUD settings, centered radar, and fixed zoom behavior; map or HUD
changes can require recalibration.

Validation: normal CMake build, radar projection tests, seven-tab menu smoke
tests at two viewport sizes, and profile round-trip tests (including persisted
calibration height) passed. Live screenshots confirmed close marker alignment
on Dust II; other resolutions were checked mathematically, not in live CS2.
