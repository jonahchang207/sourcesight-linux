# SourceSight Linux — Progress & Next Steps

## Current State (August 26, 2026)

### Release: v0.3.0
GitHub release: https://github.com/jonahchang207/sourcesight-linux/releases/tag/v0.3.0

---

## CRITICAL: Player ESP Stopped Working (NEW ISSUE — UNRESOLVED)

After the skin changer offset fix commit (5ce84ee), the player ESP stopped rendering players. All diagnostics show `snap=0 alive=0 enemies=0`. This means:
- The entity list pointer IS resolving (no longer 0x0)
- But player updates are all failing (0 succeeded, 64 failed)
- The entity handle resolution with the new +16 offset may be incorrect

**Root cause hypothesis:** The +16 offset added to entity bucket access in Player.cpp and SkinChanger.cpp may be wrong for this CS2 build. The old +0x0 offset was working for player resolution but the skin changer offsets were wrong. Now the skin offsets are fixed but the +16 broke player resolution.

**Key evidence:** Before the +16 change, ESP worked (players rendered). After +16, ESP broke. The skin changer offsets are from a Windows-based UC thread — the Linux entity list structure may differ.

**What to try:**
1. Revert entity resolution to +0x0 (remove the +16) — this was working for ESP
2. The skin changer should still work with +0x0 if the entity list pointer from our pattern scan already accounts for the +16 offset
3. If skins still don't work with +0x0, the issue is the skin offsets themselves, not entity resolution

---

## Skin Changer — Offsets Fixed But Skins Not Visually Appearing

The offsets were corrected from a verified UC working external skin changer:

| Offset | Old (wrong) | New (correct) |
|--------|-------------|---------------|
| `m_pWeaponServices` | 0x1190 | 0x11A8 |
| `m_AttributeManager` | 0x1130 | 0x1148 |
| `m_iItemDefinitionIndex` | 0x10C2 | 0x1BA |
| `m_nFallbackPaintKit` | 0x31D8 | 0x15F8 |
| `m_flFallbackWear` | 0x31DC | 0x1600 |
| `m_nFallbackSeed` | 0x31E0 | 0x15FC |
| `m_nFallbackStatTrak` | 0x31E4 | 0x1604 |
| `m_iItemIDHigh` | 0x2FC0 | 0x1D0 |
| `m_OriginalOwnerXuidLow` | 0x31F4 | 0x15F0 |

**What's needed:** The SkinDatabase.hpp has hardcoded paint kit IDs that may be wrong. Need to verify the actual paint kit IDs for each weapon from CS2's item schema. The skin changer writes paint kit IDs but if they're wrong, the game shows default skins or no skin.

**Paint kit ID source:** CS2 stores these in `items_game.txt` (inside `pak01_dir.vpk`). The paint kit IDs are the `id` fields in the `paint_kits` section. Need to extract and map them.

---

## What Was Fixed This Session

### 1. Entity List Resolution (CRITICAL BUG)
- `game.UpdateEntityList()` was stuck on `#endif` preprocessor line — never executed
- Fixed by putting it on its own line after `#endif`
- Added `Dumper::RescanEntityList()` with 3 alternative patterns

### 2. Skin Changer — Complete Offset Overhaul
- All offsets corrected from UC working external skin changer
- Entity resolution changed to +16 offset (BROKE ESP — needs revert)

### 3. Bullet Tracers
- `RenderBulletTracers()` was defined but never called from render loop
- Added call inside per-player iteration

### 4. Bomb Timer & Pulsing Border
- Removed `!bomb.pos.length()` guard that blocked rendering on first plant tick
- Added bomb timer text in ESP (countdown next to C4 icon)
- Added pulsing red screen-edge border (1-4Hz, syncs with bomb timer)

### 5. Overlay Focus
- Linux: queries `hyprctl activewindow -j` to check if CS2 has focus
- Added `Window::SetVisible()` for cross-platform show/hide

### 6. Scroll Wheel Fix
- Intercepted GLFW scroll callback, only feeds to ImGui when menu open
- Prevents overlay from capturing scroll events that cause jumping

### 7. Triggerbot
- Auto-fires when crosshair on enemy (hold Left Alt)
- Configurable delay, burst, weapon filter
- Uses XTest for click injection

### 8. Aimbot Wall Tracking Fix
- Removed pass-through fallback that allowed aiming through walls
- Map raytrace visibility check now always enforced

### 9. Glassmorphism Menu Redesign
- Frosted glass cards, sapphire blue accents
- Collapsible sections, animated tab transitions
- All original functionality preserved

---

## What Still Needs Fixing

### CRITICAL: ESP Broken by +16 Entity Resolution Change
Revert `Player.cpp` and `SkinChanger.cpp` entity resolution back to `+0x0` offset. The ESP was working before this change. The skin changer offsets are the real fix, not the entity resolution change.

### Skin Paint Kit IDs Need Verification
The `SkinDatabase.hpp` has hardcoded paint kit IDs. These need to be verified against CS2's actual `items_game.txt`. Wrong IDs = default skins showing.

**How to get correct paint kit IDs:**
1. Extract `items_game.txt` from CS2's `pak01_dir.vpk` using source2viewer
2. Find the `paint_kits` section
3. Map weapon names to paint kit IDs
4. Update `SkinDatabase.hpp` with correct IDs

### Delta Tick Force-Update
The UC working code sets `delta_tick = -1` on the network game client to force a skin refresh. This hasn't been implemented yet. Without it, skins may only apply once out of 10 attempts.

---

## Offsets (Current — `src/core/offsets/Offsets.hpp`)

### Global (pattern-scanned)
```
entityList signature: "48 8B 3D ?? ?? ?? ?? 48 85 FF 0F 94 C0 83 FE FE"
viewMatrix signature: "C6 83 ?? ?? 00 00 01 4C 8D 05"
```

### Controller
```
m_hPawn = 0x83C
m_steamID = 0x900
m_iszPlayerName = 0x874
m_bIsLocalPlayerController = 0x908
m_pInGameMoneyServices = 0x990
m_iAccount = 0x40
```

### Pawn
```
m_vOldOrigin = 0x1340
m_iHealth = 0x4BC
m_iTeamNum = 0x557
m_pGameSceneNode = 0x4A0
m_angEyeAngles = 0x41E0
m_pWeaponServices = 0x11A8
m_hActiveWeapon = 0x60
m_WeaponCount = 0x50
m_hMyWeapons = 0x58
m_AttributeManager = 0x1148
m_Item = 0x50
m_iItemDefinitionIndex = 0x1BA
m_iItemIDHigh = 0x1D0 (on C_EconItemView)
m_nFallbackPaintKit = 0x15F8
m_flFallbackWear = 0x1600
m_nFallbackSeed = 0x15FC
m_nFallbackStatTrak = 0x1604
m_OriginalOwnerXuidLow = 0x15F0
m_iAccountID = 0x1D8
m_pViewModelServices = 0x1368
m_hViewModel = 0x40
m_pGameSceneNode = 0x4A0
```

### Entity Resolution
```
entity_list + 0x10 + 0x8 * ((handle & 0x7FFF) >> 9) = bucket
bucket + 0x70 * (handle & 0x1FF) = entity
```
**NOTE: The +0x10 offset BROKE ESP. Revert to +0x0 if ESP was working before.**

---

## Build & Run

```bash
cd /home/jchang/Documents/Progects/sourcesight-linux/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./sourcesight  # Run from terminal to see logs
```

## Key Files

```
src/core/offsets/Offsets.hpp          — All offsets
src/core/engine/classes/SkinChanger.cpp — Skin application logic
src/core/engine/classes/SkinDatabase.hpp — Paint kit IDs per weapon
src/core/engine/classes/Player.cpp    — Player entity resolution
src/core/engine/classes/MapRaytrace.cpp — KD-tree raytrace
src/core/input/MouseAim.cpp          — Aimbot with visibility check
src/core/input/Triggerbot.cpp        — Triggerbot
src/gui/frontend/menu/Menu.cpp       — Glassmorphism menu
src/gui/frontend/esp/Esp.cpp         — ESP rendering
src/gui/frontend/overlays/Overlays.cpp — Bomb timer, border, radar
```
