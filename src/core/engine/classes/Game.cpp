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

    // Read pawn entity list (for C_CSPlayerPawn entities)
    if (offsets::pawnEntityList != 0) {
        const uintptr_t pel_ptr = p->read<DWORD64>(client.base + offsets::pawnEntityList);
        if (pel_ptr != 0) {
            this->pawn_entity_list = pel_ptr;
            static bool pel_logged = false;
            if (!pel_logged) {
                pel_logged = true;
                LOGF(INFO, "[game] pawn entity list resolved: pel=0x{:X}", this->pawn_entity_list);
            }
        }
    }

    return this->entity_list != 0;
}