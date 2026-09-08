# SourceSight v0.6.0 — Graphite Studio

- Rebuilt graphite menu, aperture identity, distinct category icons, padded
  cards, switches and profile controls.
- World wireframe controls: color, opacity, detail and distance. FOV unchanged.
- Transparent graphite overlay surfaces and radar background opacity.
- Background mesh loading, retries, map-change cancellation, immutable snapshots,
  validated geometry, spatial culling, clipped edges and bounded drawing work.
- Batched mesh writes with publication only after successful completion.
- Removed SkinChanger. Fixed fresh-clone geometry-library integration.
- Updated proprietary personal-use terms and retained third-party exceptions.

## Validation and limits

Local release build and geometry regressions passed. An earlier synthetic
Dust II run loaded 420,394 triangles in approximately 116 ms and averaged
0.35 ms per wireframe frame. These are local measurements, not live-game
guarantees or comparisons against another version.

Exact dot alignment over CS2's minimap remains unfinished. Live-game display
validation and anti-cheat compatibility are not claimed.
The x86-64 binary requires glibc 2.43+ and compatible Arch Linux libraries.
Game map data, kernel modules, personal configs and logs are not bundled.
