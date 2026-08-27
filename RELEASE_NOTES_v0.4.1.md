# SourceSight Linux v0.4.1

## 🎯 Player ESP Fixed (Critical)

Player ESP stopped rendering players after the skin-changer offset work. Two
compounding regressions caused it:

### 1. Wrong member offsets (introduced by "Fix Linux CS2 signatures and offsets")
That commit replaced the verified Linux member offsets with stale
Windows-era values. Every player then read the wrong fields, so everyone
looked dead and the ESP skipped them:

- `m_iHealth = 0x4BC` instead of **0x34C** → health always read 0 →
  `alive=false` for every player → nothing rendered
- `m_iTeamNum = 0x557` instead of **0x3E7** → team read 0 → `enemies=0`
- `m_hPawn = 0x83C` instead of **0x6BC** (controller → pawn handle)
- `m_vOldOrigin = 0x1340` instead of **0x13B8**
- `m_pGameSceneNode = 0x4A0` instead of **0x330**
- `m_pWeaponServices = 0x11A8` instead of **0x1208**, plus weapon, bomb,
  skin-changer and controller fields — full corrected list in
  `src/core/offsets/Offsets.hpp`

### 2. Broken entity-handle validation in `Game::ResolveHandle()`
A check was added that the entity slot's `+0x10` field equals the handle
being resolved. On this build that slot field is **not** the entity handle,
so the check rejected every real entity: pawn and weapon resolution both
failed and all player updates failed (`0 succeeded, 64 failed`).

**Fix:** removed the `slot + 0x10` validation. Resolution is the direct
bucket read that was proven working:
`chunk = entity_list[ (handle >> 9) & 0x3F ]`,
`entity = *(chunk + 0x70 * (handle & 0x1FF))` (instance pointer at slot +0x0).

### Also fixed
- **Map name:** `CGlobalVarsBase::currentMapName` is a *pointer* at
  `globalVars + 0x188` (0x180 is `currentMap`), so `map=''` never resolved —
  map name now loads (enables aimbot wall-check raytrace + watermark).
- **Skin changer weapon walk:** `m_hMyWeapons` is a
  `C_NetworkUtlVectorBase` (size at +0x0, heap element pointer at +0x8), not
  an inline array — the loop now reads count/pointer correctly.

### Diagnostics
One-shot `[player] diag:` / `[weapon] diag:` logs in `Player.cpp` and
`Weapon.cpp` report the first failing update stage (controller / pawn /
controller update / pawn update / weapon) with raw addresses, so future
offset regressions are identifiable from a single run.

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
git clone --recursive -b omarchy-port https://github.com/jonahchang207/sourcesight-linux
cd sourcesight-linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/sourcesight   # run from the repo root (needs config.json + maps/ next to the binary)
```

## ⚠️ Known Issues
- `weaponC4` carrier pattern still missing on Linux — C4-carrier ESP flag is
  disabled until a pattern is found with a live game
- Skin Database paint kit IDs still need verification against `items_game.txt`
  (extract from `pak01_dir.vpk` using source2viewer)
- Map raytrace files (*.tri) are large (>50MB) — consider Git LFS for future
- Offsets verified against the installed CS2 build (Steam buildid 24934554,
  2026-08-25, game builds 14177/14178); newer game updates may shift member
  offsets again (global signatures re-scan automatically)

## 🎮 Requirements
- CS2 on Linux (Steam)
- Root/sudo for kernel mouse (/dev/person-mouse)
- GLFW, OpenGL 3.3+, ImGui
