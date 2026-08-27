#pragma once

#include <cstdint>

class Game {
public:
    Game() {};

    bool Update();
    bool UpdateMatrix();
    bool UpdateEntityList();

    // Resolves a CS2 entity handle (controller, pawn or weapon) to an entity
    // address through the single global entity list (Linux layout):
    //
    //   chunk = *(entity_list + 8 * ((handle >> 9) & 0x3F))   (bucket ptr array at +0x0)
    //   slot  = chunk + 0x70 * (handle & 0x1FF)
    //   entity = *(void**)slot                           (instance pointer at +0x0)
    //
    // This mirrors the bucket access performed by the Linux build of
    // CGameEntitySystem::GetBaseEntity(). Handles 0xFFFFFFFF/-2 are
    // sentinels. (Windows keeps the bucket array at entity_list + 0x10.)
    static uintptr_t ResolveHandle(uintptr_t entity_list, std::uint32_t handle);

public:
    view_matrix_t view_matrix;

    uintptr_t entity_list;        // Global entity list base (bucket-pointer array on Linux)
    uintptr_t list_entry;         // Bucket 0 of the global entity list (*(entity_list + 0x0))
private:
    uintptr_t address;
};