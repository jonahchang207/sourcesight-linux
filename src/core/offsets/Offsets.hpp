namespace offsets
{
	// libclient.so
	inline DWORD entityList;
	inline DWORD pawnEntityList;  // Separate entity list for pawns (C_CSPlayerPawn)
	inline DWORD viewMatrix;
	inline DWORD localPlayerController;
	inline DWORD globalVars;
	inline DWORD plantedC4;
	inline DWORD weaponC4;

	// libengine2.so
	inline DWORD buildNumber;

	namespace controller {
		constexpr std::ptrdiff_t m_iPing = 0x9B0; // uint32
		constexpr std::ptrdiff_t m_hPawn = 0x83C; // CHandle<C_BasePlayerPawn>
		constexpr std::ptrdiff_t m_steamID = 0x900; // uint64
		constexpr std::ptrdiff_t m_iszPlayerName = 0x874; // char[128]
		constexpr std::ptrdiff_t m_bIsLocalPlayerController = 0x908; // bool
		constexpr std::ptrdiff_t m_pInGameMoneyServices = 0x990; // CCSPlayerController_InGameMoneyServices*
		constexpr std::ptrdiff_t m_iAccount = 0x40; // int32 - CCSPlayerController_InGameMoneyServices 
	}		namespace pawn {
			constexpr std::ptrdiff_t m_vOldOrigin = 0x1340; // Vector
			constexpr std::ptrdiff_t m_iHealth = 0x4BC; // int32
			constexpr std::ptrdiff_t m_iTeamNum = 0x557; // uint8
			constexpr std::ptrdiff_t m_bIsScoped = 0x2B00; // bool
			constexpr std::ptrdiff_t m_ArmorValue = 0x2B2C; // int32
			constexpr std::ptrdiff_t m_bIsDefusing = 0x2B02; // bool
			constexpr std::ptrdiff_t m_vecAbsVelocity = 0x568; // Vector

			constexpr std::ptrdiff_t m_pGameSceneNode = 0x4A0; // CGameSceneNode*
			constexpr std::ptrdiff_t m_angEyeAngles = 0x41E0; // QAngle (pitch, yaw, roll) - C_CSPlayerPawn
			
			constexpr std::ptrdiff_t m_entitySpottedState = 0x2AE8; // EntitySpottedState_t
			constexpr std::ptrdiff_t m_bSpottedByMask = 0xC; // uint32[2] - EntitySpottedState_t
			
			constexpr std::ptrdiff_t m_flFlashOverlayAlpha = 0x13A4; // float32 - C_CSPlayerPawnBase 
			
			constexpr std::ptrdiff_t m_pWeaponServices = 0x11A8; // CPlayer_WeaponServices*
			constexpr std::ptrdiff_t m_hActiveWeapon = 0x60; // CHandle<C_BasePlayerWeapon> - CPlayer_WeaponServices
			constexpr std::ptrdiff_t m_WeaponCount = 0x50; // int32 - weapon list size
			constexpr std::ptrdiff_t m_hMyWeapons = 0x58; // weapon handle array start
			constexpr std::ptrdiff_t m_AttributeManager = 0x1148; // C_AttributeContainer - C_EconEntity
			constexpr std::ptrdiff_t m_Item = 0x50; // C_EconItemView - C_AttributeContainer
			constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA; // uint16 - C_EconItemView
			constexpr std::ptrdiff_t m_iClip1 = 0x2590; // int32 - C_BasePlayerWeapon
			constexpr std::ptrdiff_t m_bInReload = 0x26A4; // bool - C_CSWeaponBase
			constexpr std::ptrdiff_t m_pObserverServices = 0x11A8; // CPlayer_ObserverServices*
			constexpr std::ptrdiff_t m_pViewModelServices = 0x1368; // CPlayer_ViewModelServices*
			constexpr std::ptrdiff_t m_hViewModel = 0x40; // CHandle - view model

			// Skin changer netvars (C_BasePlayerWeapon / C_EconEntity fallback fields)
			// These are RELATIVE to the weapon entity base (not to m_AttributeManager + m_Item)
			constexpr std::ptrdiff_t m_nFallbackPaintKit = 0x15F8; // int32 - paint kit index
			constexpr std::ptrdiff_t m_flFallbackWear = 0x1600; // float - wear (0=FN, 1=BS)
			constexpr std::ptrdiff_t m_nFallbackSeed = 0x15FC; // int32 - pattern seed
			constexpr std::ptrdiff_t m_nFallbackStatTrak = 0x1604; // int32 - stattrak kills (-1 = none)
			constexpr std::ptrdiff_t m_iItemIDHigh = 0x1D0; // int32 - on C_EconItemView sub-object
			constexpr std::ptrdiff_t m_iAccountID = 0x1D8; // int32 - account ID for ownership
			constexpr std::ptrdiff_t m_OriginalOwnerXuidLow = 0x15F0; // int32 - ownership bypass
		}

	namespace bomb {
		constexpr std::ptrdiff_t m_isPlanted = 0x8; // unk
		constexpr std::ptrdiff_t m_bC4Activated = 0x1170; // bool
		constexpr std::ptrdiff_t m_nBombSite = 0x112C; // int32

		constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8; // VectorWS - CGameSceneNode 
	}

	namespace bone {
		constexpr std::ptrdiff_t m_modelState = 0x140; // CModelState
	}

	namespace observerServices {
		constexpr std::ptrdiff_t m_iObserverMode = 0x48;
		constexpr std::ptrdiff_t m_hObserverTarget = 0x4C;
	}

	namespace global {
		constexpr std::ptrdiff_t maxClients = 0x10;
		constexpr std::ptrdiff_t currentMapName = 0x180;
		constexpr std::ptrdiff_t currentTime = 0x2C;
	}

	namespace signatures
	{
		// A pattern plus where its 4-byte RIP displacement lives. The dumper
		// resolves the target as: match + *(match + disp_offset) + instr_len.
		struct Signature
		{
			const char* bytes;       // pattern string, '??' are wildcards
			int8_t disp_offset = 3;  // byte offset of the disp32 from the pattern start
			int8_t instr_len = 7;    // distance from the pattern start to the next instruction
		};

		// Linux patterns, taken from the a2x/cs2-dumper linux branch (config.json) and
		// verified against the installed CS2 Linux build (Steam buildid 24934554, 2026-08-25).
		inline const Signature viewMatrix = { "C6 83 ?? ?? 00 00 01 4C 8D 05", 10, 14 };
		inline const Signature globalVars = { "48 8D 05 ?? ?? ?? ?? 48 8B 00 8B 40 44 F3", 3, 7 };
		inline const Signature entityList = { "48 8B 3D ?? ?? ?? ?? 48 85 FF 0F 94 C0 83 FE FE", 3, 7 };
		inline const Signature pawnEntityList = { "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 01 FF 50", 3, 7 };
		inline const Signature localPlayerController = { "48 83 3D ?? ?? ?? ?? 00 0F 95 C0 C3", 3, 8 };
		inline const Signature plantedC4 = { "48 8D 35 ?? ?? ?? ?? 66 0F EF C0 C6 05 ?? ?? ?? ?? 01 48 8D 3D", 3, 14 };

		// The C4-carrier global has no known Linux pattern yet (a2x/cs2-dumper does not
		// dump it for Linux). An empty pattern makes the scan fail gracefully and the
		// C4-carrier ESP is skipped (see Bomb.cpp). TODO: find it with a live game.
		inline const Signature weaponC4 = { "" };

		inline const Signature buildNumber = { "89 15 ?? ?? ?? ?? 48 83 C4 08 5B 5D C3", 2, 6 };
	}
}
