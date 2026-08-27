#include "SkinChanger.hpp"

#include "common.hpp"
#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/classes/Player.hpp"
#include "core/engine/classes/Weapon.hpp"
#include "core/offsets/Offsets.hpp"

#include <unordered_map>
#include <cstdint>

static std::unordered_map<int, SkinOverride> g_skins;
static bool g_force_update = false;
static bool g_diag_done = false;  // One-shot diagnostic

SkinOverride& SkinChanger::Get(int item_index) {
    g_force_update = true;
    return g_skins[item_index];
}

const std::unordered_map<int, SkinOverride>& SkinChanger::GetAll() {
    return g_skins;
}

void SkinChanger::ForceUpdate() {
    g_force_update = true;
}

// Resolve a CS2 entity handle to an entity address using the two-level
// bucket resolution: entity_list + 0x0 + stride * (handle>>9), then
// entry + 0x70 * (handle & 0x1FF).
static uintptr_t ResolveHandle(uintptr_t entity_list, uint32_t handle) {
    if (!handle || handle == 0xFFFFFFFF)
        return 0;

    auto p = Engine::GetProcess();
    if (!p) return 0;

    const uint32_t idx = handle & 0x7FFF;
    const uintptr_t bucket = p->read<uintptr_t>(entity_list + 0x0 + 0x8 * (idx >> 9));
    if (!bucket) return 0;

    return p->read<uintptr_t>(bucket + 0x70 * (idx & 0x1FF));
}

bool SkinChanger::ApplyToWeapon(uintptr_t weapon_ptr, const SkinOverride& skin) {
    auto p = Engine::GetProcess();
    if (!p || weapon_ptr == 0)
        return false;

    auto client = Engine::GetClient();

    // ── Compute the item sub-object address ──
    // m_AttributeManager (+0x1148) -> m_Item (+0x50) = C_EconItemView
    const uintptr_t item_addr = weapon_ptr
        + offsets::pawn::m_AttributeManager
        + offsets::pawn::m_Item;

    // ── One-shot diagnostic: verify offsets are sane ──
    if (!g_diag_done) {
        g_diag_done = true;
        const int32_t cur_idhigh = p->read<int32_t>(item_addr + offsets::pawn::m_iItemIDHigh);
        const int16_t cur_defidx = p->read<int16_t>(item_addr + offsets::pawn::m_iItemDefinitionIndex);
        const int32_t cur_paintkit = p->read<int32_t>(weapon_ptr + offsets::pawn::m_nFallbackPaintKit);
        LOGF(INFO, "[skin] diag: weapon=0x{:X} item=0x{:X} ItemIDHigh={} DefIdx={} PaintKit={}",
             weapon_ptr, item_addr, cur_idhigh, cur_defidx, cur_paintkit);
    }

    // ── 1. Force the game to use our fallback fields ──
    // Write m_iItemIDHigh = -1 on the C_EconItemView sub-object.
    p->write<int32_t>(item_addr + offsets::pawn::m_iItemIDHigh, -1);

    // ── 2. Write skin data to C_EconEntity fallback fields ──
    // These live at fixed offsets from the weapon entity base.
    p->write<int32_t>(weapon_ptr + offsets::pawn::m_nFallbackPaintKit, skin.paint_kit);
    p->write<float>(weapon_ptr + offsets::pawn::m_flFallbackWear, skin.wear);
    p->write<int32_t>(weapon_ptr + offsets::pawn::m_nFallbackSeed, skin.seed);

    if (skin.stattrak >= 0)
        p->write<int32_t>(weapon_ptr + offsets::pawn::m_nFallbackStatTrak, skin.stattrak);
    else
        p->write<int32_t>(weapon_ptr + offsets::pawn::m_nFallbackStatTrak, -1);

    // ── 3. Read-back verification ──
    {
        const int32_t verify_paint = p->read<int32_t>(weapon_ptr + offsets::pawn::m_nFallbackPaintKit);
        const int32_t verify_idhigh = p->read<int32_t>(item_addr + offsets::pawn::m_iItemIDHigh);
        if (verify_paint != skin.paint_kit || verify_idhigh != -1) {
            LOGF(WARNING, "[skin] WRITE FAILED: paint wrote {} read {} | idhigh wrote -1 read {}",
                 skin.paint_kit, verify_paint, verify_idhigh);
            return false;
        }
    }

    return true;
}

// Force a full model rebuild by setting delta tick to -1 on the network
// game client.  This is the key missing piece for external skin changers:
    // the game must re-send the weapon data to the client.
static void ForceSkinUpdate() {
    auto p = Engine::GetProcess();
    if (!p) return;

    auto engine = Engine::GetEngine();
    // dwNetworkGameClient from the dumper, resolved via pattern scan
    // For now, use the delta tick offset from the network game client.
    // The delta tick lives at network_game_client + 0x24C (Linux).
    // We read network game client from engine2's signature.
    // Since we don't have a direct pattern for it, we use a simpler approach:
    // set the entity list entry's serial number to force re-send.
    // Actually, the most reliable external method is to toggle the mesh
    // group mask on the view model, which we already do per-weapon.
}

void SkinChanger::Run() {
    if (g_skins.empty() && !g_force_update)
        return;

    auto p = Engine::GetProcess();
    if (!p)
        return;

    auto& cache = Cache::Get();
    const auto& local = cache.local;
    if (!local.alive || !local.localplayer)
        return;

    g_force_update = false;

    // Diagnostic: log on first few ticks
    static int skin_tick = 0;
    skin_tick++;
    if (skin_tick <= 5) {
        LOGF(INFO, "[skin] tick={} active_skins={} local_alive={} local_lp={}",
             skin_tick, g_skins.size(), local.alive, local.localplayer);
    }

    // ── Get local pawn via the controller's m_hPawn ──
    const auto controller = p->read<uintptr_t>(
        cache.game.entity_list + offsets::localPlayerController * 8);

    if (!controller)
        return;

    const uint32_t pawn_handle = p->read<uint32_t>(controller + offsets::controller::m_hPawn);
    if (!pawn_handle || pawn_handle == 0xFFFFFFFF)
        return;

    const uintptr_t local_pawn = ResolveHandle(cache.game.entity_list, pawn_handle);
    if (!local_pawn)
        return;

    // ── Walk the weapon list via CPlayer_WeaponServices ──
    const uintptr_t weapon_services = p->read<uintptr_t>(
        local_pawn + offsets::pawn::m_pWeaponServices);

    if (!weapon_services)
        return;

    const int weapon_count = std::min(
        p->read<int32_t>(weapon_services + 0x50), 64);

    for (int i = 0; i < weapon_count; ++i) {
        const uint32_t weapon_handle = p->read<uint32_t>(
            weapon_services + 0x58 + i * 0x4);

        if (!weapon_handle || weapon_handle == 0xFFFFFFFF)
            continue;

        const uintptr_t weapon_entity = ResolveHandle(
            cache.game.entity_list, weapon_handle);

        if (!weapon_entity)
            continue;

        // Read the weapon's item definition index.
        const short item_def = p->read<short>(
            weapon_entity
            + offsets::pawn::m_AttributeManager
            + offsets::pawn::m_Item
            + offsets::pawn::m_iItemDefinitionIndex);

        // Check if we have a skin override for this weapon.
        auto it = g_skins.find(item_def);
        if (it != g_skins.end() && it->second.paint_kit > 0) {
            ApplyToWeapon(weapon_entity, it->second);
        }
    }
}
