#include "Game.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"

bool Game::Update() {
	if (!Engine::GetProcess())
		return false;	
	
	if (!UpdateMatrix()) {
		LOGF(FATAL, "Failed to update view matrix");
		return false;
	}

	// No need to be updated along with the view matrix
	//if (!UpdateEntityList()) {
	//	LOGF(FATAL, "Failed to update entity list");
	//	return false;
	//}

	return true;
}

bool Game::UpdateMatrix() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	this->view_matrix = p->read<view_matrix_t>(client.base + offsets::viewMatrix);

	return true;
}

bool Game::UpdateEntityList() {
    auto p = Engine::GetProcess();
    auto client = Engine::GetClient();

    // Re-read the entity list pointer every tick.  The pointer may not be
    // ready at startup, and on some CS2 builds the pattern offset can be
    // stale.  Re-reading ensures we pick it up as soon as it becomes valid.
    const uintptr_t el_ptr = p->read<DWORD64>(client.base + offsets::entityList);

    if (el_ptr != 0) {
        this->entity_list = el_ptr;
        // On the Linux build the dwEntityList global holds the bucket-array
        // base: bucket k is a pointer at entity_list + 8*k, and each bucket
        // holds 512 CEntityIdentity slots (stride 0x70) whose first qword is
        // the entity pointer. bucket 0 is the first entry (entity_list + 0x0).
        // (The +0x10 offset is the Windows layout and reads garbage here.)
        this->list_entry = p->read<DWORD64>(this->entity_list + 0x0);

        static bool logged = false;
        if (!logged) {
            logged = true;
            LOGF(INFO, "[game] entity list resolved: el=0x{:X} le=0x{:X}",
                this->entity_list, this->list_entry);
        }
    } else {
        // Entity list pointer is null — try to re-scan the pattern.
        static int null_ticks = 0;
        null_ticks++;
        if (null_ticks == 1) {
            LOGF(WARNING, "[game] entity list is NULL — attempting pattern re-scan");
            Dumper::RescanEntityList();
        }
        if (null_ticks <= 3 || null_ticks % 100 == 0) {
            LOGF(WARNING, "[game] entity list still NULL (tick {}) — "
                "offset=0x{:X} client=0x{:X}",
                null_ticks, offsets::entityList, client.base);
        }
    }

    return this->entity_list != 0;
}

uintptr_t Game::ResolveHandle(uintptr_t entity_list, std::uint32_t handle)
{
    // Handle sentinels, exactly as CS2 itself treats them:
    // 0xFFFFFFFF == invalid, 0xFFFFFFFE == entity deleted in progress.
    if (!handle || handle == 0xFFFFFFFF || handle == 0xFFFFFFFE || !entity_list)
        return 0;

    auto p = Engine::GetProcess();
    if (!p)
        return 0;

    const std::uint32_t idx = handle & 0x7FFF; // low 15 bits: entity index

    // Bucket-pointer array is inline at entity_list + 0x0 on the Linux
    // build (bucket k at +8*k); each bucket holds 512 slots with stride 0x70
    // and the entity instance pointer sits at the start of its slot (+0x0).
    const uintptr_t chunk = p->read<uintptr_t>(entity_list + 8 * ((idx >> 9) & 0x3F));
    if (!chunk)
        return 0;

    const uintptr_t slot = chunk + 0x70 * (idx & 0x1FF);

    return p->read<uintptr_t>(slot);
}
