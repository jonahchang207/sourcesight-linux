#include "SkinChanger.hpp"

#include "common.hpp"
#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/classes/Player.hpp"
#include "core/engine/classes/Weapon.hpp"
#include "core/offsets/Offsets.hpp"

#include <unordered_map>

static std::unordered_map<int, SkinOverride> g_skins;

SkinOverride& SkinChanger::Get(int item_index) {
    return g_skins[item_index];
}

const std::unordered_map<int, SkinOverride>& SkinChanger::GetAll() {
    return g_skins;
}

bool SkinChanger::ApplyToWeapon(uintptr_t weapon_ptr, const SkinOverride& skin) {
    auto p = Engine::GetProcess();
    if (!p || weapon_ptr == 0)
        return false;

    // The weapon entity base is the pawn address.  Skin netvars live at fixed
    // offsets from this base.  We need to set m_iItemIDHigh to a non-zero
    // value so the game reads our fallback fields instead of the inventory.

    // Generate a unique item ID high based on entity index to avoid collisions.
    const int entity_index = p->read<int>(weapon_ptr + 0x10); // m_angOverlayAct
    const int item_id_high = 0x1000 + (entity_index * 7) & 0xFFFF;

    p->write<int>(weapon_ptr + offsets::pawn::m_iItemIDHigh, item_id_high);
    p->write<int>(weapon_ptr + offsets::pawn::m_OriginalOwnerXuidLow, 0);

    if (skin.paint_kit > 0) {
        p->write<int>(weapon_ptr + offsets::pawn::m_nFallbackPaintKit, skin.paint_kit);
        p->write<float>(weapon_ptr + offsets::pawn::m_flFallbackWear, skin.wear);
        p->write<int>(weapon_ptr + offsets::pawn::m_nFallbackSeed, skin.seed);
    }

    if (skin.stattrak >= 0)
        p->write<int>(weapon_ptr + offsets::pawn::m_nFallbackStatTrak, skin.stattrak);
    else
        p->write<int>(weapon_ptr + offsets::pawn::m_nFallbackStatTrak, -1);

    return true;
}

void SkinChanger::Run() {
    if (g_skins.empty())
        return;

    auto p = Engine::GetProcess();
    if (!p)
        return;

    auto& cache = Cache::Get();
    const auto& local = cache.local;
    if (!local.alive || !local.localplayer)
        return;

    // Get the local pawn address.
    const auto controller = p->read<uintptr_t>(
        cache.game.entity_list + offsets::localPlayerController * 8);

    if (!controller)
        return;

    const uint32_t pawn_handle = p->read<uint32_t>(controller + offsets::controller::m_hPawn);
    if (!pawn_handle || pawn_handle == 0xFFFFFFFF)
        return;

    const uintptr_t local_pawn = p->read<uintptr_t>(
        cache.game.entity_list + (pawn_handle & 0xFFF) * 8 + 0x10);

    if (!local_pawn)
        return;

    // Walk the weapon list via m_pWeaponServices -> m_hActiveWeapon.
    const uintptr_t weapon_services = p->read<uintptr_t>(
        local_pawn + offsets::pawn::m_pWeaponServices);

    if (!weapon_services)
        return;

    // Iterate weapons in the active weapon's slot chain.  We read up to 8
    // weapon handles from the weapon list (most players have 1-4 weapons).
    const uintptr_t weapon_list_base = p->read<uintptr_t>(weapon_services + 0x40);
    const int weapon_count = std::min(p->read<int>(weapon_services + 0x48), 8);

    for (int i = 0; i < weapon_count; ++i) {
        const uint32_t weapon_handle = p->read<uint32_t>(weapon_list_base + i * 0x4);
        if (!weapon_handle || weapon_handle == 0xFFFFFFFF)
            continue;

        const uintptr_t weapon_entity = p->read<uintptr_t>(
            cache.game.entity_list + (weapon_handle & 0xFFF) * 8 + 0x10);

        if (!weapon_entity)
            continue;

        // Read the weapon's item definition index.
        const uintptr_t item_addr = weapon_entity
            + offsets::pawn::m_AttributeManager
            + offsets::pawn::m_Item;
        const short item_def = p->read<short>(
            item_addr + offsets::pawn::m_iItemDefinitionIndex);

        // Check if we have a skin override for this weapon.
        auto it = g_skins.find(item_def);
        if (it != g_skins.end() && it->second.paint_kit > 0)
            ApplyToWeapon(weapon_entity, it->second);
    }
}
