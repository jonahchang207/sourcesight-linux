#pragma once

namespace cfg {
	inline bool enabled = true;

	namespace esp {
		inline bool team = true;

		inline bool box = true;
		inline bool box_filled = false;
		inline float box_fill_alpha = 0.12f;
		inline float box_thickness = 1.0f;
		inline float skeleton_thickness = 1.5f;
		inline float head_tracker_size = 6.0f;
		inline float tracer_thickness = 1.0f;
		inline bool armor = true;
		inline bool health = true;
		inline bool skeleton = true;
		inline bool head_tracker = true;
		inline bool head_tracker_filled = false;
		inline bool health_number = false;

		inline bool spotted = false;

		inline bool tracers = false;

		namespace bullet_tracer {
			inline bool enabled = false;
			// Max trace distance when the shot does not hit a player (world hits
			// cannot be traced from an external process).
			inline float length = 300.0f;
			// How long the tracer stays visible before disappearing.
			inline float duration = 5.0f;
			// Forward extension from the hand grip to approximate the barrel tip.
			inline float muzzle_offset = 45.0f;
			inline float thickness = 1.5f;

			inline color_t team{ 0.f, 1.f, 0.5f, 0.6f };
			inline color_t enemy{ 1.f, 0.3f, 0.3f, 0.6f };
		}
		
		inline bool bomb = true;

		namespace flags {
			inline bool name = true;
			inline bool ping = true;
			inline bool weapon = false;
			inline bool ammo = false;
			inline bool reloading = false;
			inline bool defusing = false;
			inline bool money = false;
			inline bool flashed = false;
			inline bool scoped = false;
			inline bool has_c4 = false;
		}

		namespace colors {
			inline color_t box_team{ 0.f, 1.f, 0.29f, 0.5f };
			inline color_t box_enemy{ 1.f, 0.f, 0.f, 0.5f };

			inline color_t skeleton_team{ 0.f, 1.f, 0.f, 0.5f };
			inline color_t skeleton_enemy{ 1.f, 0.f, 0.f, 0.5f };

			inline color_t tracker_team{ 1.f, 1.f, 1.f, 0.5f };
			inline color_t tracker_enemy{ 1.f, 0.25f, 0.25f, 0.5f };

			inline color_t tracer_team{ 0.f, 1.f, 0.f, 0.5f };
			inline color_t tracer_enemy{ 1.f, 0.f, 0.f, 0.5f };

			inline color_t bomb{ 1.f, 0.84f, 0.f, 1.f };
			
			namespace flags {
				inline color_t flashed_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t flashed_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t reloading_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t reloading_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t defusing_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t defusing_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t scoped_team{ 1.f, 1.f, 1.f, 0.5f };
				inline color_t scoped_enemy{ 1.f, 1.f, 1.f, 0.8f };

				inline color_t c4_team{ 1.f, 0.84f, 0.f, 1.f };
				inline color_t c4_enemy{ 1.f, 0.84f, 0.f, 1.f };
			}
			
		}

	}

	namespace world {
		namespace spectators {
			inline bool enabled = false;

			inline bool detailed = false;
			inline bool self_only = true;

			inline Vec2_t pos{ 10.f, 100.f };
		}

		namespace bomb {
			inline bool location = true;
			inline bool timer = true;
			inline Vec2_t pos{ 10.f, 300.f };
		}

		namespace crosshair {
			inline bool enabled = false;
			inline bool sniper_only = true;
			inline bool center_dot = false;
			inline bool outline = true;
			inline float gap = 6.0f;
			inline float length = 6.0f;
			inline float thickness = 1.0f;
			inline float center_dot_size = 1.5f;
			inline float outline_thickness = 1.0f;
			inline color_t color{ 1.f, 1.f, 1.f, 1.f };
		}

		namespace radar {
			inline bool enabled = true;
			inline bool no_rotate = false;
			inline float range = 2000.f;
			inline Vec2_t pos{ 10.f, 10.f };
			inline Vec2_t size{ 200.f, 200.f };
		}

		namespace velocity {
			inline bool enabled = false;
			inline int sample_rate = 35;
			inline float sample_length = 5.f;

			inline Vec2_t size{ 400.f, 100.f };
			inline Vec2_t pos{ 10.f, 400.f };
		}
	}

	namespace settings {
		inline bool watermark = true;
		inline bool streamproof = false;
		inline bool vsync = false;
		inline bool free_cpu = true;
	}

	namespace macro {
		inline bool awp_quickswitch = false;
		inline int delay_ms = 100;
	}

	// Kernel mouse aiming: screen-space target tracking through
	// /dev/person-mouse.  See core/input/MouseAim for the algorithms.
	namespace aim {
		inline bool enabled = false;
		inline bool game_mode = true;
		inline bool aim_at_enemies = true;
		inline bool hotkey = true;

		// 0 = head, 1 = body (chest), 2 = legs, 3 = in-between body & head (neck/spine1)
		inline int target_part = 3;

		// Tuned for smooth, human-like aim that looks legit.
		inline float deadzone = 1.0f;
		inline float max_delta = 12.0f;
		inline float speed = 650.0f;
		inline float fov_radius = 350.0f;
	}

	// Not stored, just for testing
	namespace dev {
		inline bool console = true;
		inline int open_menu_key = false;
		inline int cache_refresh_rate = 5;
		inline bool force_show_flags = false;
	}
}