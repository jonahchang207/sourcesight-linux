# Prompt for Continuing SourceSight Linux Development

You are continuing development on **SourceSight Linux** — an external CS2 cheat overlay (ESP, aimbot, triggerbot, skin changer) built with C++20, GLFW/OpenGL, ImGui, and X11. The project is at `/home/jchang/Documents/Progects/sourcesight-linux/`.

## CRITICAL: Two Blocking Issues to Fix

### Issue 1: Player ESP Broken (HIGHEST PRIORITY)
After changing entity handle resolution from `+0x0` to `+0x10` offset, the player ESP stopped working. Logs show `el=0x...` (entity list is non-zero) but `players: 0 succeeded, 64 failed`.

**The fix:** Revert `src/core/engine/classes/Player.cpp` line ~54 and `src/core/engine/classes/SkinChanger.cpp` line ~37 back to `+0x0` offset for entity bucket access. The ESP was working with `+0x0`. The entity list pointer from our pattern scan already accounts for any necessary offset.

Change this:
```cpp
auto entity_pawn_list_entry = p->read<uintptr_t>(this->entity_list + 0x10 + 0x8 * ((entity_pawn_address & 0x7FFF) >> 9));
```
Back to:
```cpp
auto entity_pawn_list_entry = p->read<uintptr_t>(this->entity_list + 0x0 + 0x8 * ((entity_pawn_address & 0x7FFF) >> 9));
```

Do the same in SkinChanger.cpp's `ResolveHandle()` function.

### Issue 2: Skin Changer — Paint Kit IDs Need Verification
The skin changer writes paint kit IDs to memory but the IDs in `src/core/engine/classes/SkinDatabase.hpp` are hardcoded guesses. Wrong IDs = default skins.

**How to fix:**
1. Extract `items_game.txt` from CS2's `pak01_dir.vpk` using source2viewer
2. Find the `paint_kits` section — each entry has an `id` field and a `name` field
3. Map weapon item definition indices to their valid paint kit IDs
4. Update `SkinDatabase.hpp` with correct IDs

The UC working code uses paint kit IDs like:
- AK-47 (ID 7): paint kit 433 (Neon Rider), 180 (Vulcan)
- AWP (ID 9): paint kit 344 (Dragon Lore), 504
- USP-S (ID 61): paint kit 504
- Glock (ID 4): paint kit 437

**Key offset for skin application (from verified UC working external skin changer):**
```cpp
// Write to weapon entity directly (NOT to item_addr):
m_nFallbackPaintKit = 0x15F8  // int32
m_nFallbackSeed = 0x15FC       // int32  
m_flFallbackWear = 0x1600     // float
m_nFallbackStatTrak = 0x1604  // int32

// Write to C_EconItemView sub-object (weapon + m_AttributeManager + m_Item):
m_iItemIDHigh = 0x1D0          // int32, set to -1

// After applying skins, force refresh by setting delta_tick to -1:
// engine2.dll + dwNetworkGameClient + 0x24C = -1 (Linux)
```

**The mesh group mask toggle approach may not work for visual refresh.** The UC code uses `delta_tick = -1` instead. Consider implementing this.

## Project Structure

```
src/core/offsets/Offsets.hpp          — All memory offsets
src/core/engine/classes/SkinChanger.cpp — Skin application logic  
src/core/engine/classes/SkinDatabase.hpp — Paint kit IDs per weapon (NEEDS CORRECT IDS)
src/core/engine/classes/Player.cpp    — Player entity resolution (REVERT +0x10 to +0x0)
src/core/engine/classes/Bomb.cpp      — Bomb timer logic
src/core/engine/classes/MapRaytrace.cpp — KD-tree raytrace for visibility
src/core/engine/cache/Cache.cpp       — Main data cache (entity list, players)
src/core/engine/Engine.cpp            — Engine loop
src/core/input/MouseAim.cpp          — Aimbot with visibility check
src/core/input/Triggerbot.cpp        — Triggerbot
src/core/input/KernelMouse.cpp       — Kernel mouse for aimbot movement
src/gui/frontend/menu/Menu.cpp       — Glassmorphism menu UI
src/gui/frontend/esp/Esp.cpp         — ESP rendering (box, skeleton, tracers)
src/gui/frontend/overlays/Overlays.cpp — Bomb timer, radar, watermark, pulsing border
src/gui/renderer/Renderer.cpp        — Main render loop, focus detection
src/gui/renderer/window/WindowLinux.cpp — GLFW window, input handling
src/config/Config.cpp                — Config save/load
src/config/Current.hpp               — Config defaults
```

## Build Commands
```bash
cd /home/jchang/Documents/Progects/sourcesight-linux/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./sourcesight
```

## After Fixing
1. Build and verify no compilation errors
2. Commit with message describing the fix
3. Push to `omarchy-port` branch
4. Create release notes in `RELEASE_NOTES_v0.4.0.md`
5. Build Release binary
6. Create GitHub release with `gh release create v0.4.0 --title "SourceSight Linux v0.4.0" --notes-file RELEASE_NOTES_v0.4.0.md build/sourcesight`
