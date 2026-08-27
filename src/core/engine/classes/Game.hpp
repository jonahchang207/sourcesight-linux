#pragma once

class Game {
public:
    Game() {};

    bool Update();
    bool UpdateMatrix();
    bool UpdateEntityList();

public:
    view_matrix_t view_matrix;

    uintptr_t entity_list;        // Controller entity list (CEntityIdentity** for controllers)
    uintptr_t pawn_entity_list;   // Pawn entity list (CEntityIdentity** for pawns)
    uintptr_t list_entry;         // First bucket of controller entity list
private:
    uintptr_t address;
};