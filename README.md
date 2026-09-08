<div align="center">

![SourceSight — Graphite Studio](assets/brand/banner.svg)

**A Linux-native CS2 overlay with a graphite control workspace.**

[Download v0.6.0](https://github.com/jonahchang207/sourcesight-linux/releases/tag/v0.6.0) ·
[Release notes](RELEASE_NOTES_v0.6.0.md) · [License](LICENSE)

</div>

## Graphite Studio

SourceSight combines a native Dear ImGui interface with player/world overlays,
map collision visualization, input controls and saved profiles.
Maintained by **Jonah Chang**.

![The graphite workspace](assets/brand/menu.png)

### New in 0.6.0

- Rebuilt menu with aperture branding, distinct icons, padded cards, switches
  and a persistent profile/save area.
- **World → Map geometry:** wireframe opacity, color, detail and distance.
  Lower detail reduces the maximum number of edges drawn per frame.
- Background geometry loading, automatic retries, immutable mesh snapshots,
  file validation, spatial culling and camera-plane/screen-edge clipping.
- Transparent graphite overlay surfaces and adjustable radar opacity.
- SkinChanger removed from the implementation, menu and configuration.

## Download and run

Download `sourcesight-v0.6.0-linux-x86_64.tar.gz` and its checksum from the
[release page](https://github.com/jonahchang207/sourcesight-linux/releases/tag/v0.6.0).
Extract it, open a terminal inside the extracted directory, then run:

```sh
./sourcesight
```

The binary is built on Arch Linux x86-64 and dynamically links system libraries.
It requires **glibc 2.43+**, compatible libstdc++, GLFW 3.4+, libcurl, OpenGL and
X11/Xext/Xi/Xtst. It is not a universal Linux binary; older distributions may
need an unmodified local build.

Use a windowed/borderless CS2 mode supported by your compositor. Process access
permissions depend on your setup. The repository's `drivers/` directory contains
the separate GPL-2.0 input driver; no precompiled kernel module is shipped.

No anti-cheat compatibility or account-safety guarantee is made.
SourceSight is not affiliated with Valve.

## Controls and settings

| Tab | Controls |
| --- | --- |
| Player | Boxes, skeletons, head tracking, tracers, health and player information |
| World | Bomb, spectators, crosshair, radar, velocity and map wireframe |
| Aim | Target selection, FOV, smoothing, weapon multipliers and input settings |
| Trigger | Activation, timing, bursts and weapon filters |
| Macro | Quick-switch settings |
| Sound ESP | Footsteps, gunshots, colors, distance and fade |
| Settings | Profiles and application preferences |

**Insert** opens the menu. **F9** is the panic key when enabled.
**MB5 / F10** toggle aim. **End** saves and exits.
Profiles live in `configs/` under the working directory. Use **Save profile**
to persist changes. Releases contain no personal profiles, logs or credentials.

## Map geometry

Place compatible raw triangle files in `maps/<map-name>.tri` beside the executable.
For example: `maps/de_dust2.tri`. Each triangle is three little-endian XYZ float
vectors, 36 bytes total, with no header.

SourceSight searches the configured directory, beside the executable and one
directory above it for source-tree builds. Missing maps retry every 30 seconds.
The World card reports loaded geometry and its triangle count.

Game-derived map assets are not included in the binary release and are not
covered by SourceSight's license. Obtain compatible data from your installation
using appropriate tools and permissions. Automatic extraction is best-effort;
compressed resources may require Source 2 Viewer or another compatible extractor.

## Build an unmodified copy

Requires CMake 3.24+, C++20 with `std::format`, and the libraries above.
On Arch Linux:

```sh
sudo pacman -S --needed base-devel cmake git curl glfw-x11 libx11 libxext libxi libxtst mesa ttf-dejavu
git clone --recurse-submodules https://github.com/jonahchang207/sourcesight-linux.git
cd sourcesight-linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/sourcesight
```

To run geometry regressions, install Node.js and execute:

```sh
node tools/test-map-geometry.mjs
```

Tests cover clipping, visibility, invalid files, cancellation, concurrent mesh
snapshots, zero opacity and detail budgets, without launching CS2.

## Current limits

- The radar remains a separate overlay. Exact dot alignment with the built-in
  CS2 minimap is **not implemented** in this release.
- Wireframes follow the current view matrix; FOV behavior is unchanged.
  Dense scenes may omit farther edges when the detail budget is exhausted.
- Meshes represent static collision, not smoke or every dynamic object.
- Compilation and standalone ImGui/geometry checks passed locally.
  Live in-game alignment/rendering has not been verified for this release.

## License and development

Original SourceSight material is **proprietary, source-visible software**,
not MIT/open-source software. Unmodified personal, noncommercial use and local
compilation are permitted. Selling, redistribution, modifications, mods and
derivative products require Jonah Chang's written permission.

Third-party licenses, the separate driver, earlier valid license grants and
GitHub's public-repository permissions are exceptions.
Read [LICENSE](LICENSE), [third-party notices](THIRD_PARTY_NOTICES.md), and the
[maintainer-only development policy](CONTRIBUTING.md).
