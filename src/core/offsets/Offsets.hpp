namespace offsets
{
	// libclient.so
	inline DWORD entityList;
	inline DWORD viewMatrix;
	inline DWORD localPlayerController;
	inline DWORD globalVars;
	inline DWORD plantedC4;
	inline DWORD weaponC4;

	// libengine2.so
	inline DWORD buildNumber;

	// Member offsets verified against the installed CS2 Linux build
	// (Steam buildid 24934554, 2026-08-25) and cross-checked against the
	// community schema dumps for game builds 14177 and 14178.
	namespace controller {
		constexpr std::ptrdiff_t m_iPing = 0x830; // uint32
		constexpr std::ptrdiff_t m_hPawn = 0x6BC; // CHandle<C_BasePlayerPawn>
		constexpr std::ptrdiff_t m_steamID = 0x780; // uint64
		constexpr std::ptrdiff_t m_iszPlayerName = 0x6F4; // char[128]
		constexpr std::ptrdiff_t m_bIsLocalPlayerController = 0x788; // bool
		constexpr std::ptrdiff_t m_pInGameMoneyServices = 0x810; // CCSPlayerController_InGameMoneyServices*
		constexpr std::ptrdiff_t m_iAccount = 0x40; // int32 - CCSPlayerController_InGameMoneyServices
	}
	namespace pawn {
		constexpr std::ptrdiff_t m_vOldOrigin = 0x13B8; // Vector
		constexpr std::ptrdiff_t m_iHealth = 0x34C; // int32
		constexpr std::ptrdiff_t m_iTeamNum = 0x3E7; // uint8
		constexpr std::ptrdiff_t m_bIsScoped = 0x1C78; // bool
		constexpr std::ptrdiff_t m_ArmorValue = 0x1CA4; // int32
		constexpr std::ptrdiff_t m_bIsDefusing = 0x1C7A; // bool
		constexpr std::ptrdiff_t m_vecAbsVelocity = 0x3F8; // Vector

		constexpr std::ptrdiff_t m_pGameSceneNode = 0x330; // CGameSceneNode*
		constexpr std::ptrdiff_t m_angEyeAngles = 0x3350; // QAngle (pitch, yaw, roll) - C_CSPlayerPawn

		constexpr std::ptrdiff_t m_entitySpottedState = 0x1C60; // EntitySpottedState_t - C_CSPlayerPawn
		constexpr std::ptrdiff_t m_bSpottedByMask = 0xC; // uint32[2] - EntitySpottedState_t

		constexpr std::ptrdiff_t m_flFlashOverlayAlpha = 0x141C; // float32 - C_CSPlayerPawnBase

		constexpr std::ptrdiff_t m_pWeaponServices = 0x1208; // CPlayer_WeaponServices*
		constexpr std::ptrdiff_t m_hActiveWeapon = 0x60; // CHandle<C_BasePlayerWeapon> - CPlayer_WeaponServices
		constexpr std::ptrdiff_t m_WeaponCount = 0x48; // int32 - C_NetworkUtlVectorBase size (m_hMyWeapons + 0x0)
		constexpr std::ptrdiff_t m_hMyWeapons = 0x48; // C_NetworkUtlVectorBase<CHandle<C_BasePlayerWeapon>>
		constexpr std::ptrdiff_t m_AttributeManager = 0x11A8; // C_AttributeContainer - C_EconEntity
		constexpr std::ptrdiff_t m_Item = 0x50; // C_EconItemView - C_AttributeContainer
		constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA; // uint16 - C_EconItemView
		constexpr std::ptrdiff_t m_iClip1 = 0x1700; // int32 - C_BasePlayerWeapon
		constexpr std::ptrdiff_t m_bInReload = 0x1814; // bool - C_CSWeaponBase
		constexpr std::ptrdiff_t m_pObserverServices = 0x1220; // CPlayer_ObserverServices*
		constexpr std::ptrdiff_t m_pViewModelServices = 0x1368; // CPlayer_ViewModelServices*
		constexpr std::ptrdiff_t m_hViewModel = 0x40; // CHandle - view model

		// Skin changer netvars (C_EconEntity / C_EconItemView fallback fields)
		constexpr std::ptrdiff_t m_nFallbackPaintKit = 0x1680; // int32 - paint kit index
		constexpr std::ptrdiff_t m_flFallbackWear = 0x1688; // float - wear (0=FN, 1=BS)
		constexpr std::ptrdiff_t m_nFallbackSeed = 0x1684; // int32 - pattern seed
		constexpr std::ptrdiff_t m_nFallbackStatTrak = 0x168C; // int32 - stattrak kills (-1 = none)
		constexpr std::ptrdiff_t m_iItemIDHigh = 0x1D0; // int32 - on C_EconItemView sub-object
		constexpr std::ptrdiff_t m_iAccountID = 0x1D8; // int32 - account ID for ownership
		constexpr std::ptrdiff_t m_OriginalOwnerXuidLow = 0x1678; // int32 - ownership bypass
	}

	namespace bomb {
		constexpr std::ptrdiff_t m_isPlanted = 0x8; // unk
		constexpr std::ptrdiff_t m_bC4Activated = 0x11E8; // bool
		constexpr std::ptrdiff_t m_nBombSite = 0x11A4; // int32

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
		// CGlobalVarsBase::currentMapName — pointer to the map name string
		// (currentMap is the uint64 at 0x180, the name pointer follows at 0x188).
		constexpr std::ptrdiff_t currentMapName = 0x188;
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
		inline const Signature localPlayerController = { "48 83 3D ?? ?? ?? ?? 00 0F 95 C0 C3", 3, 8 };
		inline const Signature plantedC4 = { "48 8D 35 ?? ?? ?? ?? 66 0F EF C0 C6 05 ?? ?? ?? ?? 01 48 8D 3D", 3, 14 };

		// The C4-carrier global has no known Linux pattern yet (a2x/cs2-dumper does not
		// dump it for Linux). An empty pattern makes the scan fail gracefully and the
		// C4-carrier ESP is skipped (see Bomb.cpp). TODO: find it with a live game.
		inline const Signature weaponC4 = { "" };

		inline const Signature buildNumber = { "89 15 ?? ?? ?? ?? 48 83 C4 08 5B 5D C3", 2, 6 };
	}
}
