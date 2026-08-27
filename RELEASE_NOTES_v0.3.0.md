# SourceSight Linux v0.3.0

**Release Date:** August 26, 2026

## What's New

### Triggerbot
- Auto-fires when crosshair is on an enemy (hold Left Alt to activate)
- Configurable fire delay, burst count, and burst delay
- Weapon filter: pistols only, rifles only, or all weapons
- Visible-only mode (only fires at spotted enemies)
- Target part selection (head/body/legs/neck) shared with aimbot
- Full menu UI with glassmorphism sapphire styling

### Scroll Wheel Fix
- Intercepted GLFW scroll callback to prevent overlay from capturing scroll events
- Scroll wheel now passes through to CS2 when menu is closed
- Fixes the issue where scroll wheel down caused jumping

## Bug Fixes

### Skin Changer — Complete Offset Overhaul
All skin changer offsets were corrected based on verified working external skin changer code:

| Offset | Old | New |
|--------|-----|-----|
| `m_pWeaponServices` | 0x1190 | 0x11A8 |
| `m_AttributeManager` | 0x1130 | 0x1148 |
| `m_iItemDefinitionIndex` | 0x10C2 | 0x1BA |
| `m_nFallbackPaintKit` | 0x31D8 | 0x15F8 |
| `m_flFallbackWear` | 0x31DC | 0x1600 |
| `m_nFallbackSeed` | 0x31E0 | 0x15FC |
| `m_nFallbackStatTrak` | 0x31E4 | 0x1604 |
| `m_iItemIDHigh` | 0x2FC0 | 0x1D0 |
| `m_OriginalOwnerXuidLow` | 0x31F4 | 0x15F0 |

Entity handle resolution fixed: added +16 offset for bucket access.

### Aimbot — Wall Tracking Fix
- Removed pass-through fallback that allowed aiming through walls when `visible_only` was off
- Map raytrace visibility check is now **always enforced** (blocks through walls regardless of setting)
- Player occlusion check prevents aiming through other players
- `visible_only` now additionally requires CS2 spotted state

### Other Fixes
- Entity list never updating (was stuck on `#endif` preprocessor line)
- Bullet tracers never rendering (function defined but never called)
- Bomb timer not showing (race condition with position read)
- Overlay not hiding when CS2 unfocused (Linux: hyprctl query)

## Files
- `sourcesight` — Linux x86_64 binary
