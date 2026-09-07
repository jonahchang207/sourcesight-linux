# SourceSight

> **External CS2 Overlay & Aim Assist** — Linux native, kernel-level input, VAC-safe architecture.

<div align="center">

![Linux](https://img.shields.io/badge/OS-Linux-blue?style=for-the-badge&logo=linux)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus)
![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)
![Build](https://img.shields.io/badge/Build-passing-brightgreen?style=for-the-badge)

</div>

---

## Features

### Visuals (ESP)
| Feature | Description |
|---------|-------------|
| **Player ESP** | Boxes, skeletons, head tracers, health/armor bars |
| **Bullet Tracers** | Real-time shot lines from gun tip → impact point |
| **Sound ESP** | Footstep circles + direction arrows, gunshot rings |
| **World ESP** | Bomb timer/location, spectator list, crosshair, radar |
| **Wireframe Debug** | Render map collision mesh (verify visibility checks) |

### Aim Assist
- **Kernel-level mouse** — no game memory writes, VAC-safe
- **FOV ring** with hysteresis (lock persists at edge)
- **Target priority**: crosshair distance / world distance / lowest HP / farthest
- **Weapon-specific multipliers** (rifle, pistol, sniper, SMG)
- **Recoil compensation** (0–100%)
- **Lead/extrapolation** for moving targets
- **Spinbot** with pitch sway + auto-fire

### Triggerbot
- Hold-to-fire (Left Alt) with visible-only check
- Burst fire + dwell delay + on-target radius
- Weapon filters (pistols/rifles only)

### Misc
- **Skin Changer** — grid browser, paint kit/wear/seed/Stattrak
- **Config Profiles** — save/load/delete JSON profiles
- **Panic Key** (F9) — instant disable everything
- **Stream-proof** overlay (compositor exclusion)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    SourceSight (External)                    │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐  │
│  │  ESP     │  │  Aim     │  │ Trigger  │  │  Skin      │  │
│  │  Overlay │  │  Assist  │  │  Bot     │  │  Changer   │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └─────┬──────┘  │
│       │             │             │             │          │
│       └─────────────┼─────────────┼─────────────┘          │
│                     ▼             ▼                        │
│            ┌──────────────────────────────┐                │
│            │       Engine Core            │                │
│            │  • Process / Memory          │                │
│            │  • Offset Dumper (auto)      │                │
│            │  • Entity Cache (5ms)        │                │
│            │  • Map Raytrace (KD-tree)    │                │
│            └──────────────┬───────────────┘                │
│                           │                                │
│                    ┌──────▼──────┐                          │
│                    │  Kernel     │                          │
│                    │  Mouse Dev  │  (/dev/person-mouse)     │
│                    └─────────────┘                          │
└─────────────────────────────────────────────────────────────┘
```

---

## Requirements

| Component | Version |
|-----------|---------|
| **OS** | Linux (tested on Arch/Ubuntu/Fedora) |
| **Kernel** | 5.15+ (for `/dev/person-mouse`) |
| **GPU Drivers** | NVIDIA 535+ / AMD Mesa 23+ |
| **Compositor** | Hyprland / KWin / wlroots-based |
| **Dependencies** | `glfw3`, `curl`, `opengl`, `x11`, `pthread` |
| **Game** | Counter-Strike 2 (Steam, Linux native) |

### Kernel Mouse Driver
```bash
# Install the kernel module (one-time)
cd drivers
sudo ./install.sh
# Adds /dev/person-mouse, udev rules, loads on boot
```

---

## Installation

### Quick Start (Pre-built)
```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/jonahchang207/sourcesight-linux
cd sourcesight-linux

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run (must be in-game)
./sourcesight
```

### From Source
```bash
# 1. Install dependencies
# Arch:    sudo pacman -S cmake gcc glfw-x11 curl xorgproto
# Ubuntu:  sudo apt install cmake g++ libglfw3-dev libcurl4-openssl-dev libx11-dev libxi-dev libxext-dev libxtst-dev

# 2. Clone
git clone --recurse-submodules https://github.com/jonahchang207/sourcesight-linux
cd sourcesight-linux

# 3. Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 4. Install kernel driver
cd ../drivers && sudo ./install.sh

# 5. Add user to input group (for MB5 hotkey)
sudo usermod -a -G input $USER
# Log out/in or reboot
```

---

## Map Collision Data (Required for Visibility Checks)

The aimbot uses **map collision meshes** for wall-penetration checks. You must extract these once per map.

### Option 1: Source 2 Viewer (Recommended)
```bash
# 1. Download Source 2 Viewer
#    https://github.com/ValveResourceFormat/ValveResourceFormat/releases

# 2. Open pak01_dir.vpk from: 
#    ~/.steam/steam/steamapps/common/Counter-Strike Global Offensive/game/csgo/

# 3. Navigate to: maps/de_dust2/world_physics.vphys_c

# 4. Right-click → Export → Save as .vphys

# 5. Convert to .tri using VPhysToOpt (built with SourceSight):
./VPhysToOpt /path/to/extracted/vphys/files/

# 6. Copy .tri files to maps/ folder next to executable:
cp *.tri maps/
```

### Option 2: Pre-extracted (Community)
Download pre-made `.tri` files from community releases and place in `maps/` folder.

### Maps Folder Structure
```
sourcesight-linux/
├── build/
│   ├── sourcesight          # Executable
│   └── maps/                # Place .tri files here
│       ├── de_dust2.tri
│       ├── de_mirage.tri
│       └── ...
```

> **Wireframe Debug**: Enable "Wireframe Map" in Player ESP menu to verify collision data is loaded correctly.

---

## Configuration

Settings are stored in `configs/<profile>.json`. Active profile tracked in `configs/meta.json`.

### Menu Access
- **INSERT** — Toggle menu
- **F9** — Panic key (disables everything instantly)
- **MB5 / F10** — Toggle aim assist

### Key Sections
| Tab | Purpose |
|-----|---------|
| **Player** | Boxes, skeletons, tracers, bullet tracers, flags, wireframe |
| **World** | Bomb, spectators, crosshair, radar, velocity graph |
| **Aim** | FOV, smoothing, weapon speeds, recoil comp, spinbot |
| **Trigger** | Visible-only, burst, delay, dwell, weapon filters |
| **Skins** | Paint kit browser with live preview |
| **Macro** | AWP quick-switch, bolt-action auto-switch |
| **Sound ESP** | Footsteps/gunshots, colors, distance, fade |
| **Settings** | Profile management, stream-proof, vsync |

---

## Visibility Check Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Aimbot shoots through walls | No `.tri` file loaded | Extract map physics (see above) |
| "No collision data" warning | Map not extracted | Run Source 2 Viewer extraction |
| Wireframe shows nothing | `.tri` missing or wrong map | Check `maps/` folder matches current map |
| Aimbot locks through smoke | Smoke not in collision mesh | Normal — smoke is not solid geometry |

**Debug Steps:**
1. Enable **Wireframe Map** in Player ESP tab
2. Load into a map (de_dust2, de_mirage, etc.)
3. You should see cyan wireframe of walls/floors
4. If empty → `.tri` file missing for that map

---

## File Structure

```
sourcesight-linux/
├── src/
│   ├── config/              # Config system (JSON profiles)
│   ├── core/
│   │   ├── engine/          # Engine, cache, dumper, classes
│   │   ├── input/           # MouseAim, Triggerbot, Spinbot
│   │   ├── memory/          # Process/memory abstraction
│   │   ├── logger/          # Async logging
│   │   └── offsets/         # Signature scanning + offsets
│   ├── gui/
│   │   ├── frontend/
│   │   │   ├── menu/        # ImGui menu (tabs, sections)
│   │   │   ├── esp/         # ESP rendering + SoundEsp
│   │   │   └── overlays/    # Notifications, spectator list
│   │   └── renderer/        # ImGui + GLFW + OpenGL backend
│   └── external/            # Submodules (imgui, json, AsyncLogger, VisCheckCS2)
├── drivers/                 # Kernel mouse driver
├── assets/fonts/            # Embedded icon fonts
├── build/                   # Build output (gitignored)
├── configs/                 # JSON profiles (gitignored)
├── maps/                    # .tri collision files (gitignored)
└── CMakeLists.txt
```

---

## Credits & References

| Project | Purpose |
|---------|---------|
| **ValveResourceFormat** | VPK/vphys parsing, Source 2 Viewer |
| **Read1dno/VisCheckCS2** | .vphys parser → .opt, BVH raycasting |
| **AtomicBool/cs2-map-parser** | .vphys → .tri converter |
| **ImGui** | Immediate mode GUI |
| **GLFW** | Window/input abstraction |
| **nlohmann/json** | JSON serialization |
| **AsyncLogger** | Thread-safe logging |

---

## Safety & Ethics

> **This project is for educational purposes only.**
>
> - External overlay only — no code injection, no memory writes
> - Kernel mouse driver simulates hardware input — no game hooks
> - Visibility checks use public map geometry — no `m_bSpottedByMask` abuse
> - **Use at your own risk.** VAC bans are possible with any cheat.
> - **Do not use on VAC-secured servers if you value your account.**

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Support

- **Issues**: [GitHub Issues](https://github.com/jonahchang207/sourcesight-linux/issues)
- **Discussions**: [GitHub Discussions](https://github.com/jonahchang207/sourcesight-linux/discussions)

---

<div align="center">

**Made with ❤️ for the Linux CS2 community**

</div>