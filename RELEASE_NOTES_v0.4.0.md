# SourceSight Linux v0.4.0

## 🎯 Major Fixes

### Player ESP Fixed (Critical)
- **Issue:** Player ESP stopped working after entity handle resolution offset was changed from `+0x0` to `+0x10`
- **Symptoms:** Logs showed `el=0x...` (entity list non-zero) but `players: 0 succeeded, 64 failed`
- **Fix:** Reverted entity bucket access offset back to `+0x0` in:
  - `src/core/engine/classes/Player.cpp` line 57 (`GetPawn()`)
  - `src/core/engine/classes/SkinChanger.cpp` line 41 (`ResolveHandle()`)
- **Root Cause:** The entity list pointer from our pattern scan already accounts for any necessary offset

### Skin Changer Offsets Verified
- All skin changer offsets confirmed to match UC working external skin changer
- Verified offsets in `src/core/offsets/Offsets.hpp`:
  - `m_nFallbackPaintKit = 0x15F8`
  - `m_nFallbackSeed = 0x15FC`
  - `m_flFallbackWear = 0x1600`
  - `m_nFallbackStatTrak = 0x1604`
  - `m_iItemIDHigh = 0x1D0` (on C_EconItemView sub-object)

## 📦 Contents
- External CS2 cheat overlay for Linux
- ESP: Box, Skeleton, Tracers, Health/Armor bars, Flags
- Aimbot: Visibility check, Smooth aim, FOV, RCS
- Triggerbot: Hitbox selection, Delay, Burst
- Skin Changer: Per-weapon paint kits, Wear, Seed, StatTrak
- Bomb Timer, Radar, Watermark, Pulsing border overlay
- Glassmorphism ImGui menu
- Kernel mouse movement for aimbot

## 🛠 Build
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./sourcesight
```

## ⚠️ Known Issues
- Skin Database paint kit IDs need verification against `items_game.txt` (extract from `pak01_dir.vpk` using source2viewer)
- Map raytrace files (*.tri) are large (>50MB) - consider Git LFS for future

## 🎮 Requirements
- CS2 on Linux (Steam)
- Root/sudo for kernel mouse (/dev/uinput)
- GLFW, OpenGL 3.3+, ImGui