#pragma once

#include <cstdint>

class Game {
public:
    Game() {};

    bool Update();
    bool UpdateMatrix();
    bool UpdateEntityList();

    // Resolves a CS2 entity handle (controller, pawn or weapon) to an entity
    // address through the single global entity list:
    //
    //   chunk = entity_list[ (handle >> 9) & 0x3F ]      (array of chunk ptrs)
    //   slot  = chunk + 0x70 * (handle & 0x1FF)
    //   entity = *(void**)slot                           (instance pointer at +0x0)
    //
    // This mirrors the bucket access CGameEntitySystem::GetBaseEntity()
    // performs on the installed libclient.so. Handles 0xFFFFFFFF/-2 are
    // sentinels. No extra handle field is validated inside the slot: the
    // layout does not store the entity handle at slot +0x10 on this build.
    static uintptr_t ResolveHandle(uintptr_t entity_list, std::uint32_t handle);

public:
    view_matrix_t view_matrix;

    uintptr_t entity_list;        // Global entity list (pointer array of chunks)
    uintptr_t list_entry;         // First bucket of the controller entity list
private:
    uintptr_t address;
};