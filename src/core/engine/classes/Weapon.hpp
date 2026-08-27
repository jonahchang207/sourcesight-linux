#pragma once
#include "core/engine/types/Weapons.hpp"

class Weapon
{
public:
	Weapon(uintptr_t entity_list, uintptr_t pawn_entity_list, int slot_index)
		: entity_list(entity_list), pawn_entity_list(pawn_entity_list), slot_index(slot_index) {}
	Weapon() 
		: item_index(-1), name("Invalid"), icon("?"), ammo(0), is_reloading(false), slot_index(0), entity_list(0), pawn_entity_list(0) { }

	bool Update();
public:
	short item_index;
	std::string name;
	const char* icon;
	int32_t ammo;
	bool is_reloading;

private:
	const char* ToString() const;
	const char* ToIcon() const;

	int slot_index;
	uintptr_t entity_list;        // Controller entity list (for compatibility)
	uintptr_t pawn_entity_list;   // Pawn entity list (for weapon handle resolution)
};

