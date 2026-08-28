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
		inline bool spotted_only = false;   // Only render spotted (visible) enemies
		inline bool distance = true;       // Show distance to player
		inline bool headshot_line = false;  // Line from crosshair to enemy head

		inline bool tracers = false;

		namespace bullet_tracer {
			inline bool enabled = false;
			inline float length = 300.0f;
			inline float duration = 5.0f;
			inline float muzzle_offset = 45.0f;
			inline float thickness = 1.5f;

			inline color_t team{ 0.f, 1.f, 0.5f, 0.6f };
			inline color_t enemy{ 1.f, 0.3f, 0.3f, 0.6f };
		}
		
		inline bool bomb = true;

		namespace flags {
			inline bool name = true;
			inline bool ping = true;
			inline bool weapon = true;        // Now on by default
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

			inline color_t headshot_line{ 1.f, 0.2f, 0.2f, 0.7f };

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
			inline bool enabled = true;       // On by default now
			inline bool sniper_only = false;   // Show for all weapons
			inline bool center_dot = true;
			inline bool outline = true;
			inline float gap = 6.0f;
			inline float length = 6.0f;
			inline float thickness = 1.0f;
			inline float center_dot_size = 2.0f;
			inline float outline_thickness = 1.0f;
			inline color_t color{ 0.f, 0.7f, 1.f, 1.f };
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
		inline bool streamproof = true;    // On by default for safety
		inline bool vsync = false;
		inline bool free_cpu = true;
		inline bool panic_key = true;      // Press F9 to disable everything
		inline bool panic_key_pressed = false; // Set by render thread, read by engine
	}

	namespace macro {
		inline bool awp_quickswitch = false;
		inline int delay_ms = 100;
		inline bool auto_switch_all = true; // Quick-switch for all bolt-action weapons
	}

	// Kernel mouse aiming.
	namespace aim {
		inline bool enabled = false;
		inline bool game_mode = true;
		inline bool aim_at_enemies = true;
		inline bool hotkey = true;            // MB5 (side button) toggles aim
		inline bool visible_only = false;   // Only aim at visible (non-occluded) targets
		inline bool auto_start = false;     // Auto-enable aim when round starts

		// Set by the render thread when the aim-toggle key is pressed; consumed
		// by MouseAim::Update (same pattern as panic_key_pressed).
		inline bool toggle_requested = false;
		// Keyboard key that toggles aim (in addition to MB5). Reliable even when
		// CS2 grabs raw mouse input (which starves the evdev listener).
		inline int toggle_key = 292;          // ImGuiKey_F10 (282 is covered by other binds)

		// 0 = head, 1 = body (chest), 2 = legs, 3 = in-between body & head
		inline int target_part = 3;

		// Target picker. Always runs inside the FOV ring; this decides which
		// visible enemy to lock onto when more than one qualifies.
		//   0 = crosshair proximity (closest to the crosshair wins)
		//   1 = distance          (nearest enemy wins)
		//   2 = health            (weakest enemy wins)
		//   3 = distance, inverse (farthest enemy wins — sniping)
		inline int priority = 0;

		// Weapon-specific speed multipliers (fraction of base speed).
		inline float rifle_mult = 1.0f;
		inline float pistol_mult = 1.2f;
		inline float sniper_mult = 0.7f;
		inline float smg_mult = 1.1f;

		inline float recoil_compensation = 0.0f; // 0-1: how much to counter recoil
		inline float target_switch_delay = 0.0f; // Seconds before switching targets

		inline float deadzone = 1.0f;
		inline float max_delta = 24.0f;         // Max pixels per frame the crosshair may move
		inline float speed = 2600.0f;           // Top approach velocity in px/second
		inline float smoothness = 0.22f;        // 0..1 proportional gain; lower = smoother
		inline float aim_smoothing = 0.50f;     // EMA weight (0..1) on the tracked aim point; 1 = raw (jittery)
		inline float lead_time = 0.25f;         // Seconds of enemy velocity extrapolation (tracking)
		inline float fov_radius = 350.0f;       // Acquisition radius (px around crosshair)
		inline float exit_fov_mult = 1.6f;      // Release happens only beyond fov*this (hysteresis)

		// Linked aim+trigger: once the locked target has been reached, release
		// the strafe keys (A/D) and fire bursts using the triggerbot burst
		// settings, until the target is lost.
		inline bool lock_burst = false;
	}

	// Continuous camera spin driven purely through the kernel mouse driver
	// (no game-memory writes). Rotating the view carries the player model
	// with it, so incoming shots fan out across a moving hitbox.
	namespace spinbot {
		inline bool enabled = false;
		inline bool shoot = true;          // Auto-fire once per enemy the crosshair sweeps over
		inline int direction = 1;          // +1 = clockwise (yaw right), -1 = counter-clockwise
		inline float speed = 1600.0f;      // px/s of horizontal mouse movement while spinning
		inline float pitch_sway = 0.0f;    // px of vertical sway per spin (0 = pure yaw)
		inline float sway_hz = 2.5f;       // sway oscillation rate
	}

	// Triggerbot: auto-fire when crosshair is on an enemy.
	namespace triggerbot {
		inline bool enabled = false;
		inline bool hotkey = true;          // Hold key to activate
		inline bool visible_only = true;    // Only fire at visible enemies
		inline int delay_ms = 0;            // Delay before firing (0 = instant)
		inline int burst_count = 1;         // Shots per trigger (1 = semi-auto)
		inline int burst_delay_ms = 80;     // Delay between burst shots
		inline float threshold = 12.0f;     // On-target radius (px) for the crosshair check
		inline int dwell_ms = 25;           // Debounce: hold on target this long before firing
		inline bool pistols_only = false;   // Only trigger with pistols
		inline bool rifles_only = false;    // Only trigger with rifles
	}

	// Bypass / anti-detection.
	namespace bypass {
		inline bool timing_jitter = true;     // Randomise write timing
		inline bool humanize_movement = true;  // Add micro-noise to mouse deltas
		inline int write_delay_min_us = 50;    // Min delay between writes (us)
		inline int write_delay_max_us = 200;   // Max delay between writes (us)
		inline float noise_amplitude = 0.3f;   // Pixels of random noise per write
	}

	// Audio feedback.
	namespace audio {
		inline bool lock_sound = false;        // Play a sound on target lock
	}

	// Not stored, just for testing
	namespace dev {
		inline bool console = true;
		inline int open_menu_key = false;
		inline int cache_refresh_rate = 5;
		inline bool force_show_flags = false;
	}
}