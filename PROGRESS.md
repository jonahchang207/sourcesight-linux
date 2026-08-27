# SourceSight Linux — Progress & Next Steps

## Current State (August 26, 2026)

### Release: v0.3.0
GitHub release: https://github.com/jonahchang207/sourcesight-linux/releases/tag/v0.3.0

---

## RESOLVED: Player ESP — root cause was Windows member offsets on the Linux build

The Aug 26 fix (b703ba2) accidentally replaced the correct **Linux** member offsets
with **Windows** dump values. The Linux build of CS2 has its own class layout
(verified: `C_BaseEntity::m_pGameSceneNode` is 0x4A0 on Linux vs 0x330 on
Windows for the same July build; controller `m_hPawn` 0x83C vs 0x6BC, etc.).
With the wrong offsets every player read garbage → `0 succeeded, 64 failed`,
health 0x00C80000, `m_pGameSceneNode`=0, `weapon_services`=0x1.

The entity list itself was fine on Linux: the `dwEntityList` global holds the
bucket-array base, bucket k at `entity_list + 8*k`, 512 slots/bucket at stride
0x70 with the entity pointer at slot +0x0 (matches the current Linux cheat
`deadlocked`). The +0x10 bucket offset is the **Windows** layout.

**Fix applied (Aug 27):** restored Linux member offsets from the a2x linux
schema dump and reverted the bucket base to `entity_list + 0x0`.

---

## Skin Changer — offsets are Linux values now

The earlier "corrected" skin offsets were taken from a Windows UC thread and are
wrong on Linux. The Linux values (a2x linux libclient.so.hpp) are:

| Offset | Linux (current) |
|--------|-----------------|
| `m_pWeaponServices` | 0x1190 (C_BasePlayerPawn) |
| `m_AttributeManager` | 0x1130 (C_EconEntity) |
| `m_iItemDefinitionIndex` | 0x10C2 (C_EconItemView) |
| `m_nFallbackPaintKit` | 0x2510 |
| `m_flFallbackWear` | 0x2518 |
| `m_nFallbackSeed` | 0x2514 |
| `m_nFallbackStatTrak` | 0x251C |
| `m_iItemIDHigh` | 0x10D8 (C_EconItemView) |
| `m_iAccountID` | 0x10E0 (C_EconItemView) |
| `m_OriginalOwnerXuidLow` | 0x2508 (C_EconEntity) |

**Remaining:** verify paint kit IDs in SkinDatabase.hpp against `items_game.txt`,
and implement the delta-tick force-update for reliable skin application.

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

### Controller (Linux)
```
m_hPawn = 0x83C            m_iPing = 0x9B0
m_steamID = 0x900          m_iszPlayerName = 0x874
m_bIsLocalPlayerController = 0x908
m_pInGameMoneyServices = 0x990
m_iAccount = 0x40
```

### Pawn (Linux)
```
m_vOldOrigin = 0x1340      m_iHealth = 0x4BC
m_iTeamNum = 0x557         m_vecAbsVelocity = 0x568
m_pGameSceneNode = 0x4A0   m_angEyeAngles = 0x41E0
m_entitySpottedState = 0x2AE8  m_bSpottedByMask = 0xC
m_flFlashOverlayAlpha = 0x13A4
m_pWeaponServices = 0x1190     m_pObserverServices = 0x11A8
m_hActiveWeapon = 0x60     m_hMyWeapons = 0x48 (C_NetworkUtlVectorBase)
m_AttributeManager = 0x1130    m_Item = 0x50
m_iItemDefinitionIndex = 0x10C2   m_iClip1 = 0x2590
m_bInReload = 0x26A4       m_bIsScoped = 0x2B00
m_bIsDefusing = 0x2B02     m_ArmorValue = 0x2B2C
skin: m_nFallbackPaintKit 0x2510, m_nFallbackSeed 0x2514,
      m_flFallbackWear 0x2518, m_nFallbackStatTrak 0x251C,
      m_OriginalOwnerXuidLow 0x2508, m_iItemIDHigh 0x10D8, m_iAccountID 0x10E0
```

### Entity Resolution (Linux)
```
entity_list + 0x0 + 0x8 * ((handle & 0x7FFF) >> 9) = bucket ptr   (bucket array at +0x0)
bucket + 0x70 * (handle & 0x1FF) = entity
```
**NOTE: On the Linux build the bucket-pointer array is at `entity_list + 0x0`.
The +0x10 base is the Windows layout — it reads NULL on Linux.**

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
