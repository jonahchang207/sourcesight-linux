#include "Esp.hpp"

#include "core/input/MouseAim.hpp"
#include "gui/renderer/Renderer.hpp"
#include "assets/fonts/WeaponIcons.h"
#include "assets/fonts/Icons.h"

#include <limits>
#include <numbers>

namespace {

// Source engine AngleVectors: yaw 0 faces +X, pitch up is negative.
Vec3_t AngleToDirection(const Vec3_t& angles) {
    const float pitch = angles.x * (std::numbers::pi_v<float> / 180.0f);
    const float yaw = angles.y * (std::numbers::pi_v<float> / 180.0f);
    return Vec3_t(
        std::cos(pitch) * std::cos(yaw),
        std::cos(pitch) * std::sin(yaw),
        -std::sin(pitch)
    ).normalized();
}

// Ray vs. axis-aligned box (slab method). Returns the entry distance t along
// the ray, or -1 when there is no hit. A hit at t == 0 means the origin is
// already inside the box.
float RayBox(const Vec3_t& origin, const Vec3_t& dir, const Vec3_t& center,
             const Vec3_t& half) {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        const float o = origin.at(axis) - center.at(axis);
        const float d = dir.at(axis);
        if (std::fabs(d) < 1e-6f) {
            if (o < -half.at(axis) || o > half.at(axis))
                return -1.0f;
        }
        else {
            float t1 = (-half.at(axis) - o) / d;
            float t2 = (half.at(axis) - o) / d;
            if (t1 > t2)
                std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax)
                return -1.0f;
        }
    }
    return std::max(0.0f, tmin);
}

// Approximate the muzzle position: the midpoint of the two hand bones (the
// weapon grip) extended forward along the aim direction. Falls back to the
// eye position when the skeleton is unavailable.
Vec3_t GunTip(const Player& player) {
    const Vec3_t forward = AngleToDirection(player.eye_angles);
    const float muzzle = std::clamp(cfg::esp::bullet_tracer::muzzle_offset, 0.0f, 200.0f);
    Vec3_t grip = player.pos + Vec3_t(0.f, 0.f, 64.f);
    if (player.bone_list.size() > static_cast<size_t>(bone_index::hand_R)) {
        grip = (player.bone_list[bone_index::hand_L].pos
                + player.bone_list[bone_index::hand_R].pos) * 0.5f;
    }
    // Slight rightward bias: the weapon sits on the right side of the model.
    const float yaw = player.eye_angles.y * (std::numbers::pi_v<float> / 180.0f);
    const Vec3_t right(-std::sin(yaw), std::cos(yaw), 0.f);
    return grip + forward * muzzle + right * 6.f;
}

// Trace a shot along ``dir`` and return the first enemy player hit (the
// shooter and their teammates excluded, the local player included as a valid
// target), or the max-trace endpoint when the shot only hits the world.
// Player boxes approximate the Source hull (32x32x72 world units, centred at
// feet + 36 up). World geometry is not traceable from an external process.
Vec3_t TraceShot(const Vec3_t& origin, const Vec3_t& dir, float max_len,
                 const std::vector<Player>& players, int shooter_index,
                 int shooter_team) {
    float best_t = max_len;
    for (const auto& target : players) {
        if (!target.alive || target.index == shooter_index)
            continue;
        // Only the shooter's enemies block the bullet (CS2 bullets stop at
        // enemy hitboxes); with no assigned team, keep every player.
        if (shooter_team != 0 && target.team == shooter_team)
            continue;
        const Vec3_t center = target.pos + Vec3_t(0.f, 0.f, 36.f);
        const float t = RayBox(origin, dir, center, Vec3_t(16.f, 16.f, 36.f));
        if (t >= 0.0f && t < best_t)
            best_t = t;
    }
    return origin + dir * best_t;
}

} // namespace

bool Esp::Init() {
	return GetInstance().InitImpl();
}

void Esp::Render() {
    return GetInstance().RenderImpl();
}

bool Esp::InitImpl() {
	auto& io = ImGui::GetIO();

	ImFontConfig file_font_cfg{};
	ImFontConfig embedded_font_cfg{};
	embedded_font_cfg.FontDataOwnedByAtlas = false;

#ifdef _WIN32
	this->font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 12.0f, &file_font_cfg);
#else
	this->font = io.Fonts->AddFontFromFileTTF("/usr/share/fonts/TTF/DejaVuSansMono.ttf", 12.0f, &file_font_cfg);
	if (!this->font)
		this->font = io.Fonts->AddFontDefault(&file_font_cfg);
#endif

	this->font_merged_icons = io.Fonts->AddFontFromMemoryTTF(
		weapon_icon_font,
		weapon_icon_font_len,
		16.0f,
		&embedded_font_cfg
	);

	embedded_font_cfg.MergeMode = true;

	static const ImWchar general_ranges[] = { 0xE100, 0xE108, 0 };
	io.Fonts->AddFontFromMemoryTTF(
		icons_font,
		icons_font_len,
		16.0f,
		&embedded_font_cfg,
		general_ranges
	);

	return this->font && this->font_merged_icons;
}

void Esp::RenderImpl() {
	// Panic key detection (must be on the render thread where ImGui is safe).
	if (cfg::settings::panic_key && ImGui::IsKeyPressed(ImGuiKey_F9))
		cfg::settings::panic_key_pressed = true;

	if (!cfg::enabled)
		return;

	auto snapshot = Cache::CopySnapshot();
	auto& game = snapshot.game;
	auto& bomb = snapshot.bomb;
	auto& local = snapshot.local;
	auto& globals = snapshot.globals;
	auto& players = snapshot.players;
	
	ImGui::PushFont(this->font);

	this->io = ImGui::GetIO();
	this->d = ImGui::GetBackgroundDrawList();

	this->matrix = game.view_matrix;

	// Diagnostic: log on FIRST render, then every ~2 seconds.
	static int esp_tick = 0;
	esp_tick++;
	static auto last_diag = std::chrono::steady_clock::now();
	auto now_diag = std::chrono::steady_clock::now();
	bool diag_now = (esp_tick <= 3) || (now_diag - last_diag > std::chrono::seconds(2));
	if (diag_now) {
		last_diag = now_diag;
		int alive_count = 0, enemy_count = 0;
		for (auto& p : players) {
			if (p.alive) alive_count++;
			if (p.alive && !p.localplayer && p.team != local.team) enemy_count++;
		}
		LOGF(INFO, "[esp] tick={} snap={} alive={} enemies={} local_hp={} vm_ok={}",
			esp_tick, players.size(), alive_count, enemy_count, local.health,
			(game.view_matrix[3][3] != 0.0f));
	}

	for (auto& player : players) {
		if (!player.alive)
			continue;

		if (player.localplayer)
			continue;

		bool mate = player.team == local.team;

		if (!cfg::esp::team && mate)
			continue;

		if (cfg::esp::spotted && !player.spotted)
			continue;

		// Do not render the player currently being spectated in first person.
		// TODO: Exception here when spectating someone
		if (
			local.observer_services.target == player.pawn_controller_addr
			&& local.observer_services.mode == ObserverMode::First
		)
			continue;

		RenderPlayerTracers(local, player, mate);
		RenderPlayer(player, mate);
	}

	RenderCrosshair(local);
	RenderBombBox(bomb);
	RenderAimFov();
	ImGui::PopFont();
}

void Esp::RenderPlayer(Player player, bool mate) {
	// Needed for flags & item sizing, so even if the box is not enabled
	// Should be calculated
	std::pair<Vec2_t, Vec2_t> bounds;
	if (!player.GetBounds(matrix, io.DisplaySize, bounds))
		return;

	// Causes hp bars across the screen when they respawn
	if (!player.alive)
		return;

	// Spotted-only filter: skip rendering enemies we cannot see.
	if (!mate && cfg::esp::spotted_only && !player.spotted)
		return;

	if (cfg::esp::box) {
		auto color = mate ? cfg::esp::colors::box_team : cfg::esp::colors::box_enemy;

		if (cfg::esp::box_filled) {
			auto fill_color = color;
			fill_color.a = std::clamp(cfg::esp::box_fill_alpha, 0.0f, 1.0f);
			d->AddRectFilled(bounds.first, bounds.second, ImColor(fill_color));
		}

		d->AddRect(
			bounds.first,
			bounds.second,
			ImColor(color),
			0.0f,
			ImDrawFlags_None,
			std::clamp(cfg::esp::box_thickness, 1.0f, 4.0f)
		);
	}

	if (cfg::esp::skeleton)
		RenderPlayerBones(player, mate);

	if (cfg::esp::head_tracker)
		RenderPlayerTracker(player, bounds, mate);

	RenderPlayerBars(player, bounds);
	RenderPlayerFlags(player, bounds, mate);

	// Headshot line: from the screen centre (crosshair) to the enemy head.
	if (cfg::esp::headshot_line && !mate &&
		player.bone_list.size() > static_cast<size_t>(bone_index::head)) {
		Vec2_t head;
		if (matrix.wts(player.bone_list[bone_index::head].pos, io.DisplaySize, head)) {
			const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
			d->AddLine(center, head, ImColor(cfg::esp::colors::headshot_line), 1.0f);
		}
	}
}

void Esp::RenderPlayerBones(Player player, bool mate) {
	auto color = mate ? cfg::esp::colors::skeleton_team : cfg::esp::colors::skeleton_enemy;

	auto bone_count = player.bone_list.size();
	for (const auto& bone : connections) {
		int first = bone[0], second = bone[1];

		if (bone_count <= first || bone_count <= second)
			continue;

		const auto& bone1 = player.bone_list[first];
		const auto& bone2 = player.bone_list[second];

		Vec2_t scb1;
		if (!matrix.wts(bone1.pos, io.DisplaySize, scb1))
			continue;

		Vec2_t scb2;
		if (!matrix.wts(bone2.pos, io.DisplaySize, scb2))
			continue;

		d->AddLine(
			scb1,
			scb2,
			ImColor(color),
			std::clamp(cfg::esp::skeleton_thickness, 1.0f, 4.0f)
		);
	}
}

void Esp::RenderPlayerTracker(Player player, std::pair<Vec2_t, Vec2_t> bounds, bool mate) {
	if (player.bone_list.size() <= bone_index::head)
		return;

	auto head_bone = player.bone_list[bone_index::head];

	Vec2_t head;
	if (!matrix.wts(head_bone.pos, io.DisplaySize, head))
		return;

	auto width = bounds.second.x - bounds.first.x;
	auto color = mate ? cfg::esp::colors::tracker_team : cfg::esp::colors::tracker_enemy;

	const float tracker_size = std::clamp(cfg::esp::head_tracker_size, 2.0f, 14.0f);
	const float radius = std::max(2.0f, width / 6.0f * (tracker_size / 6.0f));
	if (cfg::esp::head_tracker_filled)
		d->AddCircleFilled(head, radius, ImColor(color), 15);
	else
		d->AddCircle(head, radius, ImColor(color), 15, 1.0f);
}

void Esp::RenderPlayerBars(Player player, std::pair<Vec2_t, Vec2_t> bounds) {
	if (cfg::esp::health) {
		auto x_start = bounds.first.x - 4; // -4 is padding
		auto x_end = x_start - 2; // -2 is the inner space of the rect

		auto y_start = bounds.first.y;
		auto y_end = bounds.second.y;

		float height = y_end - y_start;
		float filled_height = height * (player.health / 100.0f);

		d->AddRectFilled(
			ImVec2(x_start, y_end - filled_height),
			ImVec2(x_end, y_end),
			IM_COL32(100, 255, 100, 255)
		);

		d->AddRect(
			ImVec2(x_start, y_start),
			ImVec2(x_end, y_end),
			IM_COL32(0, 0, 0, 50)
		);

		if (cfg::esp::health_number && player.health < 100) {
			auto txt = std::to_string(player.health);
			auto sz = ImGui::CalcTextSize(txt.c_str());

			d->AddText(
				Vec2_t(
					(x_start + x_end) * 0.5f - sz.x * 0.5f,
					y_end - filled_height - sz.y * 0.5f
				),
				IM_COL32(255, 255, 255, 255),
				txt.c_str()
			);
		}
	}

	if (cfg::esp::armor) {
		auto y_start = bounds.second.y + 4; // 4 is padding
		auto y_end = y_start + 2; // 2 is the inner space of the rect

		auto x_start = bounds.first.x;
		auto x_end = bounds.second.x;

		float width = x_end - x_start;
		float filled_width = width * (player.armor / 100.0f);

		d->AddRectFilled(
			ImVec2(x_start, y_start),
			ImVec2(x_start + filled_width, y_end),
			IM_COL32(150, 150, 255, 255)
		);

		d->AddRect(
			ImVec2(x_start, y_start),
			ImVec2(x_end, y_end),
			IM_COL32(0, 0, 0, 50)
		);
	}
}

void Esp::RenderPlayerFlags(Player player, std::pair<Vec2_t, Vec2_t> bounds, bool mate) {
	if (cfg::esp::flags::name) {
		auto sanitized_name = std::format("{}{}", player.name, (player.bot ? " (Bot)" : ""));
		auto name_size = ImGui::CalcTextSize(sanitized_name.data());

		d->AddText(
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - name_size.x / 2,
				bounds.first.y - 20
			), 
			IM_COL32(255, 255, 255, 255),
			sanitized_name.data()
		);
	}

	// Distance display below the name
	if (cfg::esp::distance) {
		const auto& local = Cache::Get().local;
		const float dx = player.pos.x - local.pos.x;
		const float dy = player.pos.y - local.pos.y;
		const float dz = player.pos.z - local.pos.z;
		const int dist = static_cast<int>(std::sqrt(dx*dx + dy*dy + dz*dz) * 0.01905f); // Source units to metres
		auto dist_str = std::format("{}m", dist);
		auto dist_size = ImGui::CalcTextSize(dist_str.c_str());
		d->AddText(
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - dist_size.x / 2,
				bounds.first.y - 34
			),
			IM_COL32(200, 200, 200, 200),
			dist_str.c_str()
		);
	}

	if (cfg::esp::flags::ammo && player.ammo != -1) {
		auto txt = std::to_string(player.ammo);
		auto ammo_size = ImGui::CalcTextSize(txt.c_str());

		d->AddText(
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - ammo_size.x / 2,
				bounds.second.y + 20
			),
			IM_COL32(255, 255, 255, 255),
			txt.data()
		);
	}

	int offset = 0;
	static int offset_mult = 15;

	if (cfg::esp::flags::money && player.money) {
		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			IM_COL32(255, 255, 255, 255),
			std::format("{}$", player.money).c_str()
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::ping && player.ping) {
		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			IM_COL32(255, 255, 255, 255),
			std::format("{}ms", player.ping).c_str()
		);

		offset -= offset_mult;
	}

	ImGui::PushFont(this->font_merged_icons);

	if (cfg::esp::flags::flashed && player.flashed || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::flashed_team : cfg::esp::colors::flags::flashed_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			Icons::BLIND
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::reloading && player.is_reloading || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::reloading_team : cfg::esp::colors::flags::reloading_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			Icons::RELOAD
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::defusing && player.defusing || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::defusing_team : cfg::esp::colors::flags::defusing_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			WeaponIcons::CUTTERS
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::scoped && player.scoped || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::scoped_team : cfg::esp::colors::flags::scoped_enemy;

		d->AddText(
			bounds.first - Vec2_t((bounds.first.x - bounds.second.x) - 10, offset),
			ImColor(color),
			WeaponIcons::SCOPE
		);

		offset -= offset_mult;
	}

	if (cfg::esp::flags::weapon) {
		auto weapon_size = ImGui::CalcTextSize(player.weapon.icon);

		d->AddText(
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - weapon_size.x / 2,
				bounds.second.y + 6
			),
			IM_COL32(255, 255, 255, 255),
			player.weapon.icon
		);
	}

	if (cfg::esp::flags::has_c4 && player.has_c4 || cfg::dev::force_show_flags) {
		auto color = mate ? cfg::esp::colors::flags::c4_team : cfg::esp::colors::flags::c4_enemy;

		ImGui::PushFont(this->font_merged_icons);
		auto icon_size = ImGui::CalcTextSize(WeaponIcons::C4);
		ImGui::PopFont();

		// Flash the icon if they're holding the C4, presumably planting since nobody just holds it really
		ImColor draw_color = ImColor(color);
		if (player.weapon.item_index == weapon_c4) {
			float alpha = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 8.0f);
			draw_color = ImColor(color.r, color.g, color.b, alpha);
		}

		d->AddText(
			this->font_merged_icons,
			16.0f,
			Vec2_t(
				(bounds.first.x + bounds.second.x) / 2 - icon_size.x / 2,
				bounds.first.y - 20 - icon_size.y - 2
			),
			draw_color,
			WeaponIcons::C4
		);

		offset -= offset_mult;
	}

	ImGui::PopFont();
}

void Esp::RenderBombBox(Bomb bomb) {
	if (!cfg::esp::bomb)
		return;


	if (!bomb.is_planted)
		return;

	// Bomb dimensions
	float w = 10.f, l = 5.f, h = 10.f;
	Vec3_t half_size = { w / 2.f, h / 2.f, l / 2.f };

	Vec3_t corners[8] = {
		{ bomb.pos.x - half_size.x, bomb.pos.y - half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y - half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y - half_size.y, bomb.pos.z + half_size.z },
		{ bomb.pos.x - half_size.x, bomb.pos.y - half_size.y, bomb.pos.z + half_size.z },
		{ bomb.pos.x - half_size.x, bomb.pos.y + half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y + half_size.y, bomb.pos.z - half_size.z },
		{ bomb.pos.x + half_size.x, bomb.pos.y + half_size.y, bomb.pos.z + half_size.z },
		{ bomb.pos.x - half_size.x, bomb.pos.y + half_size.y, bomb.pos.z + half_size.z },
	};

	Vec2_t projected[8];
	bool visible[8] = { false };
	int visible_count = 0;

	for (int i = 0; i < 8; ++i) {
		if (matrix.wts(corners[i], io.DisplaySize, projected[i])) {
			visible[i] = true;
			visible_count++;
		}
	}

	if (visible_count == 0)
		return;

	auto color = cfg::esp::colors::bomb;

	int edges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // Bottom
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // Top
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }  // Verticals
	};

	for (auto& edge : edges) {
		int i = edge[0];
		int j = edge[1];
		if (visible[i] && visible[j]) {
			d->AddLine(projected[i], projected[j], ImColor(color), 1.0f);
		}
	}

	Vec2_t screen;
	if (!matrix.wts(bomb.pos + Vec3_t(0, 0, 8), io.DisplaySize, screen))
		return;

	ImGui::PushFont(this->font_merged_icons);
	d->AddText(
		this->font_merged_icons,
		16.0f,
		Vec2_t(
			screen.x - 8, // lazy
			screen.y
		),
		ImColor(255, 255, 255),
		WeaponIcons::C4
	);
	ImGui::PopFont();
}

void Esp::RenderCrosshair(Player local)
{
	if (!cfg::world::crosshair::enabled || local.scoped)
		return;

	auto weapon = local.weapon;
	if (weapon.item_index == -1)
		return;

	static const std::vector<WeaponIds> valid_weapons = {
		weapon_ssg08, weapon_awp, weapon_g3sg1, weapon_scar20
	};

	if (cfg::world::crosshair::sniper_only &&
		std::find(valid_weapons.begin(), valid_weapons.end(), weapon.item_index) == valid_weapons.end())
		return;

	const ImVec2 center(
		floorf(io.DisplaySize.x * 0.5f),
		floorf(io.DisplaySize.y * 0.5f));
	const float gap = std::max(0.0f, cfg::world::crosshair::gap);
	const float length = std::max(0.0f, cfg::world::crosshair::length);
	const float thickness = std::max(1.0f, cfg::world::crosshair::thickness);
	const ImColor color(cfg::world::crosshair::color);

	const auto draw_line = [&](ImVec2 start, ImVec2 end) {
		if (cfg::world::crosshair::outline) {
			d->AddLine(
				start,
				end,
				IM_COL32(0, 0, 0, 220),
				thickness + 2.0f * std::max(0.0f, cfg::world::crosshair::outline_thickness)
			);
		}
		d->AddLine(start, end, color, thickness);
	};

	draw_line(
		ImVec2(center.x - gap - length, center.y),
		ImVec2(center.x - gap, center.y)
	);
	draw_line(
		ImVec2(center.x + gap, center.y),
		ImVec2(center.x + gap + length, center.y)
	);
	draw_line(
		ImVec2(center.x, center.y - gap - length),
		ImVec2(center.x, center.y - gap)
	);
	draw_line(
		ImVec2(center.x, center.y + gap),
		ImVec2(center.x, center.y + gap + length)
	);

	if (cfg::world::crosshair::center_dot) {
		const float dot_size = std::max(0.5f, cfg::world::crosshair::center_dot_size);
		if (cfg::world::crosshair::outline)
			d->AddCircleFilled(center, dot_size + cfg::world::crosshair::outline_thickness, IM_COL32(0, 0, 0, 220));
		d->AddCircleFilled(center, dot_size, color, 12);
	}
}

void Esp::RenderPlayerTracers(Player source, Player player, bool mate) {
	if (!cfg::esp::tracers)
		return;

	Vec2_t screenPos;
	bool projected = matrix.wts(player.pos, io.DisplaySize, screenPos, false);

	if (!projected)
	{
		Vec3_t camPos = source.pos;
		Vec3_t dir = player.pos - camPos;

		// projection for off screen players
		Vec3_t viewDir;
		viewDir.x = matrix[0][0] * dir.x + matrix[0][1] * dir.y + matrix[0][2] * dir.z;
		viewDir.y = matrix[1][0] * dir.x + matrix[1][1] * dir.y + matrix[1][2] * dir.z;
		viewDir.z = matrix[2][0] * dir.x + matrix[2][1] * dir.y + matrix[2][2] * dir.z;

		if (viewDir.z > 0.0f)
		{
			viewDir.x = -viewDir.x;
			viewDir.y = -viewDir.y;
		}

		// normalize
		float len = sqrt(viewDir.x * viewDir.x + viewDir.y * viewDir.y);
		if (len > 0.001f)
		{
			viewDir.x /= len;
			viewDir.y /= len;
		}

		screenPos.x = io.DisplaySize.x * 0.5f + viewDir.x * io.DisplaySize.x * 0.5f;
		screenPos.y = io.DisplaySize.y * 0.5f - viewDir.y * io.DisplaySize.y * 0.5f;

		float margin = 10.f;
		screenPos.x = std::clamp(screenPos.x, margin, io.DisplaySize.x - margin);
		screenPos.y = std::clamp(screenPos.y, margin, io.DisplaySize.y - margin);
	}

	auto color = mate ? cfg::esp::colors::tracer_team : cfg::esp::colors::tracer_enemy;

	d->AddLine(
		Vec2_t(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
		screenPos,
		ImColor(color),
		std::clamp(cfg::esp::tracer_thickness, 1.0f, 4.0f)
	);
}

void Esp::RenderBulletTracers(Player player, const std::vector<Player>& players, bool mate) {
	if (!cfg::esp::bullet_tracer::enabled)
		return;

	const float now = static_cast<float>(ImGui::GetTime());

	// Detect a new shot: clip ammo dropped since the last frame we saw this
	// player. The first observation only seeds the baseline (no phantom). A
	// weapon switch also changes clip ammo, so it must not count as a shot.
	auto& prev = last_ammo[player.index];
	const bool same_weapon = prev.second == player.weapon.item_index;
	const bool ammo_dropped = same_weapon && player.ammo >= 0 && prev.first > player.ammo;
	prev = { player.ammo, player.weapon.item_index };

	if (ammo_dropped) {
		// Freeze the full segment at fire time: from the gun tip to the first
		// player hit (or the max trace distance when only the world is hit).
		const Vec3_t origin = GunTip(player);
		const Vec3_t dir = AngleToDirection(player.eye_angles);
		const float max_len = std::max(1.0f, cfg::esp::bullet_tracer::length);
		const Vec3_t end = TraceShot(origin, dir, max_len, players, player.index, player.team);

		// Bound the buffer: automatic fire at 5 s lifetime must not grow forever.
		if (tracers.size() >= 256)
			tracers.erase(tracers.begin());
		tracers.push_back({ origin, end, now, mate, player.localplayer });
	}

	// Render and expire active tracers, fading only in the final moments.
	const float duration = std::max(0.1f, cfg::esp::bullet_tracer::duration);
	const float thickness = std::clamp(cfg::esp::bullet_tracer::thickness, 1.0f, 4.0f);
	const float fade_tail = std::min(0.5f, duration * 0.25f);

	for (auto it = tracers.begin(); it != tracers.end();) {
		const float age = now - it->start_time;
		if (age > duration) {
			it = tracers.erase(it);
			continue;
		}

		// Line-of-sight gate: only show shots whose shooter we can actually
		// see on screen (in front of the camera and inside the view frustum).
		// Our own gun is always in view. World geometry is not traceable from
		// an external process, so this is the practical visibility proxy.
		if (!it->from_local) {
			Vec2_t visible;
			if (!matrix.wts(it->origin, io.DisplaySize, visible)) {
				++it;
				continue;
			}
		}

		Vec2_t a, b;
		// The endpoint may legitimately leave the screen; only the start is
		// required to stay on-screen.
		if (matrix.wts(it->origin, io.DisplaySize, a, false) &&
			matrix.wts(it->end, io.DisplaySize, b, false)) {
			auto color = it->mate ? cfg::esp::bullet_tracer::team : cfg::esp::bullet_tracer::enemy;
			// Single pass: cheap to draw, no glow layering.
			float alpha = 1.0f;
			if (age > duration - fade_tail)
				alpha = (duration - age) / fade_tail;
			color.a *= alpha;
			d->AddLine(a, b, ImColor(color), thickness);
		}
		++it;
	}
}

void Esp::RenderAimFov() {
	// Status label: always visible so the user knows if aim is live.
	const char* status = cfg::aim::enabled ? "AIM ON" : "AIM OFF";
	const ImU32 status_col = cfg::aim::enabled
		? IM_COL32(110, 255, 150, 235) : IM_COL32(150, 160, 155, 150);
	const auto status_size = ImGui::CalcTextSize(status);
	d->AddText(ImVec2(io.DisplaySize.x * 0.5f - status_size.x * 0.5f, 20.0f),
			status_col, status);

	// FOV ring: only drawn when aim is enabled.  Shows the acquisition
	// zone around the crosshair where enemies can be locked onto.
	if (!cfg::aim::enabled)
		return;

	// Scale from monitor space into overlay pixels.
	float screen_w = io.DisplaySize.x;
	float screen_h = io.DisplaySize.y;
	MouseAim::ScreenSize(screen_w, screen_h);
	const float sx = io.DisplaySize.x / std::max(1.0f, screen_w);
	const float sy = io.DisplaySize.y / std::max(1.0f, screen_h);

	// Centre of the ring: crosshair in game mode, else the tracked cursor.
	float cx = io.DisplaySize.x * 0.5f;
	float cy = io.DisplaySize.y * 0.5f;
	if (!cfg::aim::game_mode) {
		float mx, my;
		if (MouseAim::TrackedCursor(mx, my)) {
			cx = mx * sx;
			cy = my * sy;
		}
	}

	const float radius = cfg::aim::fov_radius * std::min(sx, sy);
	if (radius < 1.0f)
		return;

	// Subtle sapphire ring — thin and semi-transparent so it doesn't
	// distract, but clearly marks the detection boundary.
	const ImU32 ring = IM_COL32(80, 140, 230, 90);
	d->AddCircle(ImVec2(cx, cy), radius, ring, 64, 1.5f);
}
