# SourceSight Linux — Progress & Next Steps

## Current State (August 26, 2026)

### Critical Bug: Entity List is NULL (`el=0x0`)
The **root cause** of ESP not working is that `game.entity_list = 0x0`. Diagnostic output:
```
[cache] tick=1 el=0x0 le=0x0 mc=64 map='' c4=0x0
[cache] players: 0 succeeded, 64 failed, max_clients=64
[esp] tick=1 snap=0 alive=0 enemies=0 local_hp=0 vm_ok=true
```

**What this means:**
- The Dumper pattern scan found the entity list offset successfully ("Successfully dumped offsets...")
- BUT `p->read<uintptr_t>(client.base + offsets::entityList)` returns 0
- The view matrix IS valid (`vm_ok=true`), so `client.base` is correct
- The entity list pattern may be stale for the user's CS2 build

**What was added to fix it:**
- `Game::UpdateEntityList()` now retries every tick and calls `Dumper::RescanEntityList()` on first failure
- `Dumper::RescanEntityList()` tries 3 patterns (original + 2 alternatives)
- Extensive diagnostic logging at every level

**Next step:** User needs to run the tool again after rebuild and check if re-scan found a valid offset. If all 3 patterns fail, need to find the correct pattern for their CS2 build.

---

## What Was Built This Session

### 1. Skin Changer — Complete Rewrite (`src/core/engine/classes/SkinChanger.cpp`)
- **Fixed entity resolution**: Two-level bucket resolution (`handle & 0x7FFF >> 9` + `& 0x1FF`)
- **Fixed weapon list traversal**: Offsets `+0x50` (count) and `+0x58` (handles) for `CPlayer_WeaponServices`
- **Fixed m_iItemIDHigh write**: Now writes to `item_addr` (C_EconItemView sub-object) not directly to weapon_ptr
- **Added read-back verification**: Confirms writes actually stuck
- **Added hide/show toggle**: Mask=0 hides weapon, restore on next frame forces model rebuild
- **One-shot diagnostic logging**: Logs weapon EntityIDHigh, DefIdx, PaintKit on first apply
- **`ForceUpdate()` method**: Called from menu "Apply" button

### 2. Skin Preset Save/Load (`src/config/Config.cpp`)
- Active skins saved to `config.json` under `"skins"` key
- JSON object keyed by weapon ID (as string), each with paint_kit, wear, seed, stattrak
- Loaded on startup, `ForceUpdate()` called after load

### 3. Skin Database — Per-Weapon Presets (`src/core/engine/classes/SkinDatabase.hpp`)
- `SkinsByWeapon()` — maps weapon ID → vector of skin presets
- 30+ weapons with 15-25 skins each (rifles, SMGs, shotguns, snipers, pistols, knives)
- Category-based filtering in the menu

### 4. UI Overhaul — Grid-Based Skin Browser (`src/gui/frontend/menu/Menu.cpp`)
- Category tabs: All / Rifles / SMGs / Shotguns / Snipers / Pistols / Knives
- 100×52px weapon card grid with WeaponIcons font glyphs
- Cards glow green when skin active, blue when selected
- Right-side config panel with skin dropdown, wear/seed/stattrak, Apply/Reset
- Active skins summary at bottom

### 5. Aim Toggle Fix (`src/core/input/MouseAim.cpp`)
- Changed `g_hotkey.Sync(cfg::aim::hotkey && cfg::aim::enabled)` → `g_hotkey.Sync(cfg::aim::hotkey)`
- Hotkey thread now stays alive when aim is disabled, so MB5 can re-enable

### 6. Visibility Check — Map Raytracing (`src/core/engine/classes/MapRaytrace.cpp`)
- KD-tree acceleration against parsed `.tri` map collision files
- Möller-Trumbore ray-triangle intersection
- Auto-detects map changes from CS2's GlobalVars
- Falls back to player-only occlusion if no .tri file loaded
- Files: `MapRaytrace.hpp`, `MapRaytrace.cpp`

### 7. Hyprland Fix (`scripts/install-omarchy.sh`)
- Added `fullscreen = true` to Lua rules (was missing)

### 8. Map Extraction Tools
- `tools/extract_maps.py` — Python tool using ValveResourceFormat
- `tools/extract_maps.sh` — Bash wrapper using source2viewer + cs2-map-parser
- Supports de_dust2, de_mirage, de_inferno, de_overpass, de_nuke, de_ancient, de_anubis, de_vertigo, cs_office

### 9. Diagnostics
- Cache: Logs entity list, list_entry, max_clients, map name, player counts (first 5 ticks + every 3s)
- ESP: Logs snapshot size, alive count, enemy count, local HP, view matrix validity
- Skin: Logs active skin count, local player state (first 5 ticks)
- Game: Logs entity list resolution with retry/re-scan

---

## Offsets (Current — `src/core/offsets/Offsets.hpp`)

```
controller::m_hPawn = 0x83C
pawn::m_pWeaponServices = 0x1190
pawn::m_AttributeManager = 0x1130
pawn::m_Item = 0x50
pawn::m_iItemDefinitionIndex = 0x10C2
pawn::m_nFallbackPaintKit = 0x31D8
pawn::m_flFallbackWear = 0x31DC
pawn::m_nFallbackSeed = 0x31E0
pawn::m_nFallbackStatTrak = 0x31E4
pawn::m_iItemIDHigh = 0x2FC0 (on C_EconItemView, NOT weapon entity directly!)
pawn::m_OriginalOwnerXuidLow = 0x31F4
pawn::m_pGameSceneNode = 0x4A0
entityList signature: "48 8B 3D ?? ?? ?? ?? 48 85 FF 0F 94 C0 83 FE FE"
viewMatrix signature: "C6 83 ?? ?? 00 00 01 4C 8D 05"
```

---

## TODO — Windows Map Parser

The user is going to Windows to run the map parser. Steps:

### Option A: C# csphys-extractor (Easiest)
1. Download from unknowncheats: "CS2 vmdl_c parser" by laithiraq
2. Requires .NET Framework/.NET Core
3. Run the tool → select "All maps" → choose `.tri` format
4. Output: `.tri` files for all competitive maps
5. Copy the `.tri` files to Linux in `maps/` folder next to the SourceSight executable

### Option B: source2viewer + cs2-map-parser
1. Download source2viewer: https://github.com/ValveResourceFormat/ValveResourceFormat
2. Build cs2-map-parser: https://github.com/AtomicBool/cs2-map-parser
3. For each map:
   - Open `de_dust2.vpk` in source2viewer
   - Extract `maps/de_dust2/world_physics.vphys_c`
   - Decompress with source2viewer CLI
   - Run vphys_parser on the decompressed file
   - Output: `de_dust2.tri`

### Map files needed for the user (plays dust2 and mirage):
- `maps/de_dust2.tri`
- `maps/de_mirage.tri`

### .tri file format:
Binary file, each triangle = 9 floats (36 bytes):
```
float p1_x, p1_y, p1_z, p2_x, p2_y, p2_z, p3_x, p3_y, p3_z
```

---

## Remaining Issues

### Entity List NULL (CRITICAL)
- The Dumper finds the pattern but the read returns 0
- Re-scan mechanism added (3 patterns tried)
- If all fail: need to find the correct pattern for user's CS2 build
- Can check CS2 build with `cs2 --version` or check Steam build ID

### Skin Changer — Skins Not Visually Appearing
- Memory writes are verified (read-back check)
- Mesh group mask toggle forces visual refresh
- But user hasn't tested with the latest build yet (entity list was the blocker)
- Once entity list is fixed, test skin application

### ESP Not Rendering
- Directly caused by `el=0x0` (no entity list → no player data → nothing to render)
- Once entity list is fixed, ESP should work automatically

---

## Build & Run

```bash
cd /home/jchang/Documents/Progects/sourcesight-linux/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./sourcesight  # Run from terminal to see logs
```

## File Structure (Key Files Modified)

```
src/core/engine/classes/SkinChanger.cpp     — Rewritten with read-back, item_addr fix
src/core/engine/classes/SkinChanger.hpp     — Added ForceUpdate()
src/core/engine/classes/SkinDatabase.hpp    — Per-weapon skin presets (SkinsByWeapon)
src/core/engine/classes/MapRaytrace.cpp     — NEW: KD-tree raytrace against .tri files
src/core/engine/classes/MapRaytrace.hpp     — NEW: Map raytrace interface
src/core/engine/classes/Game.cpp            — Entity list retry + re-scan
src/core/engine/cache/Cache.cpp             — Diagnostic logging + map reload
src/core/engine/Engine.cpp                  — MapRaytrace::Init()
src/core/offsets/Dumper.cpp                 — RescanEntityList() with alt patterns
src/core/offsets/Dumper.hpp                 — RescanEntityList() declaration
src/core/offsets/Offsets.hpp                — Skin changer offsets
src/core/input/MouseAim.cpp                — Toggle fix + map raytrace vis check
src/gui/frontend/esp/Esp.cpp               — Diagnostic logging
src/gui/frontend/menu/Menu.cpp             — Grid-based skin browser UI
src/config/Config.cpp                      — Skin preset save/load
src/config/Current.hpp                     — Config defaults
scripts/install-omarchy.sh                 — Hyprland fullscreen fix
tools/extract_maps.py                      — NEW: Python map extractor
tools/extract_maps.sh                      — NEW: Bash map extractor
```
