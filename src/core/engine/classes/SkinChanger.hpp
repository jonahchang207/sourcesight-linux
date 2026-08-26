#pragma once

#include <string>
#include <unordered_map>

// A skin override for a specific weapon slot.
struct SkinOverride {
    int paint_kit = 0;      // Paint kit ID (skin)
    float wear = 0.0f;      // 0.0 = Factory New, 1.0 = Battle-Scarred
    int seed = 0;           // Pattern seed
    int stattrak = -1;      // -1 = no stattrak, >= 0 = kill count
    int quality = 4;        // 4 = Unique (normal)
};

class SkinChanger {
public:
    // Call once per engine tick from the same thread as Player::Update().
    // Writes skin overrides to all weapons held by the local player.
    static void Run();

    // Get/set skin override for a weapon item definition index.
    static SkinOverride& Get(int item_index);
    static const std::unordered_map<int, SkinOverride>& GetAll();

    // Write a single weapon's skin values. Returns true on success.
    static bool ApplyToWeapon(uintptr_t weapon_ptr, const SkinOverride& skin);
};
