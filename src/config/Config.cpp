#include "Config.hpp"
#include "core/engine/classes/SkinChanger.hpp"
#include "core/engine/classes/SkinDatabase.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>

// ─────────────────────────────────────────────────────────────────────────────
// Profile plumbing
// ─────────────────────────────────────────────────────────────────────────────

Config::Config() {
	EnsureConfigDir();

	// One-time import of a legacy single-file config into the active profile so
	// existing setups keep their settings after the move to profiles.
	const std::string active = GetActiveProfile();
	const std::string active_path = ProfilePath(active);
	std::error_code ec;
	if (!std::filesystem::exists(active_path, ec) &&
	    std::filesystem::exists("config.json", ec)) {
		std::filesystem::copy_file(
			"config.json", active_path,
			std::filesystem::copy_options::overwrite_existing, ec);
		if (!ec)
			LOGF(INFO, "Imported legacy config.json into profile '{}'", active);
	}

	EnsureMeta();
}

std::string Config::GetActiveProfile() {
	std::string name = "default";
	{
		std::ifstream f(MetaPath());
		if (f.good()) {
			try {
				json meta = json::parse(f);
				name = meta.value("active", name);
			}
			catch (...) { /* fall back to default */ }
		}
	}
	const std::string clean = SanitizeName(name);
	return clean.empty() ? "default" : clean;
}

void Config::SetActiveProfile(const std::string& name) {
	const std::string clean = SanitizeName(name);
	if (clean.empty()) return;
	EnsureConfigDir();
	json meta;
	meta["active"] = clean;
	std::ofstream f(MetaPath());
	f << std::setw(4) << meta << std::endl;
}

std::vector<std::string> Config::ListProfiles() {
	EnsureConfigDir();
	std::vector<std::string> out;
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(ProfileDir(), ec)) {
		if (!entry.is_regular_file(ec)) continue;
		if (entry.path().extension() != ".json") continue;
		if (entry.path().filename() == "meta.json") continue;
		out.push_back(entry.path().stem().string());
	}
	std::sort(out.begin(), out.end());
	return out;
}

bool Config::HasProfile(const std::string& name) {
	const std::string clean = SanitizeName(name);
	return !clean.empty() && std::filesystem::exists(ProfilePath(clean));
}

bool Config::LoadProfile(const std::string& name) {
	const std::string clean = SanitizeName(name);
	if (clean.empty()) {
		LOGF(WARNING, "Invalid profile name, refusing to load");
		return false;
	}
	SetActiveProfile(clean);
	const bool ok = GetInstance().ReadImpl(ProfilePath(clean));
	LOGF(INFO, "Loaded profile '{}'", clean);
	return ok;
}

bool Config::SaveProfile(const std::string& name) {
	const std::string clean = SanitizeName(name);
	if (clean.empty()) {
		LOGF(WARNING, "Invalid profile name, refusing to save");
		return false;
	}
	EnsureConfigDir();
	SetActiveProfile(clean);
	const bool ok = GetInstance().WriteImpl(ProfilePath(clean));
	LOGF(INFO, "Saved profile '{}'", clean);
	return ok;
}

bool Config::DeleteProfile(const std::string& name) {
	const std::string clean = SanitizeName(name);
	if (clean.empty()) return false;
	std::error_code ec;
	std::filesystem::remove(ProfilePath(clean), ec);
	if (!ec && GetActiveProfile() == clean)
		SetActiveProfile("default");
	LOGF(INFO, "Deleted profile '{}'", clean);
	return !ec;
}

bool Config::Read() {
	std::lock_guard lock(Mutex());
	return GetInstance().ReadImpl(ProfilePath(GetActiveProfile()));
}

bool Config::Write() {
	std::lock_guard lock(Mutex());
	return GetInstance().WriteImpl(ProfilePath(GetActiveProfile()));
}

std::string Config::ProfileDir() { return "configs"; }
std::string Config::MetaPath() { return ProfileDir() + "/meta.json"; }
std::string Config::ProfilePath(const std::string& name) {
	return ProfileDir() + "/" + SanitizeName(name) + ".json";
}

bool Config::EnsureConfigDir() {
	std::error_code ec;
	std::filesystem::create_directories(ProfileDir(), ec);
	return !ec;
}

void Config::EnsureMeta() {
	if (std::filesystem::exists(MetaPath())) return;
	SetActiveProfile(GetActiveProfile());
}

std::mutex& Config::Mutex() {
	static std::mutex m;
	return m;
}

// Sanitise a profile name into a safe filename component. Strips path
// separators and any character that could escape the configs/ directory, so a
// hostile or accidental name can never read/write outside of it. Returns "" for
// names that become empty (e.g. "." or "..").
std::string Config::SanitizeName(const std::string& name) {
	std::string out;
	out.reserve(name.size());
	for (const unsigned char c : name) {
		if (std::isalnum(c) || c == '-' || c == '_' || c == ' ' || c == '.')
			out += static_cast<char>(c);
	}

	auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
	const auto first = std::find_if(out.begin(), out.end(), [&](char c) { return !is_space(static_cast<unsigned char>(c)); });
	const auto last = std::find_if(out.rbegin(), out.rend(), [&](char c) { return !is_space(static_cast<unsigned char>(c)); }).base();
	if (first >= last) return "";
	std::string trimmed(first, last);
	if (trimmed == "." || trimmed == "..") return "";
	if (trimmed.size() > 40) trimmed.resize(40);
	return trimmed;
}

bool Config::ReadImpl(const std::string& path) {
	std::ifstream f(path);

	if (!f.good()) {
		LOGF(INFO, "Configuration file does not exist; creating defaults");
		WriteImpl(path);
		return false;
	}

	json data;
	try {
		data = json::parse(f);
	}
	catch (const std::exception& e) {
		LOGF(WARNING, "Failed to parse configuration file ({}); restoring defaults", e.what());
		WriteImpl(path);
		return false;
	}

	if (data.empty())
		return false;

	try {
		// general
		cfg::enabled = data.value("enabled", true);

		// esp
		cfg::esp::box = data["esp"].value("box", true);
		cfg::esp::box_filled = data["esp"].value("box_filled", false);
		cfg::esp::box_fill_alpha = data["esp"].value("box_fill_alpha", 0.12f);
		cfg::esp::box_thickness = data["esp"].value("box_thickness", 1.0f);
		cfg::esp::skeleton_thickness = data["esp"].value("skeleton_thickness", 1.5f);
		cfg::esp::head_tracker_size = data["esp"].value("head_tracker_size", 6.0f);
		cfg::esp::tracer_thickness = data["esp"].value("tracer_thickness", 1.0f);
		cfg::esp::team = data["esp"].value("team", true);
		cfg::esp::armor = data["esp"].value("armor", true);
		cfg::esp::health = data["esp"].value("health", true);
		cfg::esp::spotted = data["esp"].value("spotted", false);
		cfg::esp::spotted_only = data["esp"].value("spotted_only", false);
		cfg::esp::distance = data["esp"].value("distance", true);
		cfg::esp::headshot_line = data["esp"].value("headshot_line", false);
		cfg::esp::skeleton = data["esp"].value("skeleton", true);
		cfg::esp::head_tracker = data["esp"].value("head_tracker", true);
		cfg::esp::head_tracker_filled = data["esp"].value("head_tracker_filled", false);
		cfg::esp::health_number = data["esp"].value("health_number", false);
		cfg::esp::tracers = data["esp"].value("tracers", false);
		cfg::esp::bomb = data["esp"].value("bomb", true);

		// bullet tracer
		if (data["esp"].contains("bullet_tracer")) {
			const auto& bt = data["esp"]["bullet_tracer"];
			cfg::esp::bullet_tracer::enabled = bt.value("enabled", false);
			cfg::esp::bullet_tracer::length = bt.value("length", 300.0f);
			cfg::esp::bullet_tracer::duration = bt.value("duration", 5.0f);
			cfg::esp::bullet_tracer::muzzle_offset = bt.value("muzzle_offset", 45.0f);
			cfg::esp::bullet_tracer::thickness = bt.value("thickness", 1.5f);
			cfg::esp::bullet_tracer::team = JsonToColor(bt, "team", { 0.f, 1.f, 0.5f, 0.6f });
			cfg::esp::bullet_tracer::enemy = JsonToColor(bt, "enemy", { 1.f, 0.3f, 0.3f, 0.6f });
		}
		
		// flags
		cfg::esp::flags::name = data["esp"]["flags"].value("name", true);
		cfg::esp::flags::ping = data["esp"]["flags"].value("ping", false);
		cfg::esp::flags::money = data["esp"]["flags"].value("money", false);
		cfg::esp::flags::weapon = data["esp"]["flags"].value("weapon", false);
		cfg::esp::flags::ammo = data["esp"]["flags"].value("ammo", false);
		cfg::esp::flags::reloading = data["esp"]["flags"].value("reloading", false);
		cfg::esp::flags::scoped = data["esp"]["flags"].value("scoped", false);
		cfg::esp::flags::defusing = data["esp"]["flags"].value("defusing", false);
		cfg::esp::flags::flashed = data["esp"]["flags"].value("flashed", false);
		cfg::esp::flags::has_c4 = data["esp"]["flags"].value("has_c4", false);

		// colors
		const auto& col = data["esp"]["colors"];
		cfg::esp::colors::box_team = JsonToColor(col, "box_team", { 0.f, 1.f, 0.29f, 0.5f });
		cfg::esp::colors::box_enemy = JsonToColor(col, "box_enemy", { 1.f, 0.f, 0.f, 0.5f });

		cfg::esp::colors::skeleton_team = JsonToColor(col, "skeleton_team", { 0.f, 1.f, 0.f, 0.5f });
		cfg::esp::colors::skeleton_enemy = JsonToColor(col, "skeleton_enemy", { 1.f, 0.f, 0.f, 0.5f });

		cfg::esp::colors::tracker_team = JsonToColor(col, "tracker_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::tracker_enemy = JsonToColor(col, "tracker_enemy", { 1.f, 0.25f, 0.25f, 0.5f });

		cfg::esp::colors::tracer_team = JsonToColor(col, "tracer_team", { 0.f, 1.f, 0.f, 0.5f });
		cfg::esp::colors::tracer_enemy = JsonToColor(col, "tracer_enemy", { 1.f, 0.f, 0.f, 0.5f });

		cfg::esp::colors::bomb = JsonToColor(col, "bomb", { 1.f, 0.84f, 0.f, 1.f });	

		// flag colors
		const auto& fcol = data["esp"]["colors"]["flags"];

		cfg::esp::colors::flags::flashed_team = JsonToColor(fcol, "flashed_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::flashed_enemy = JsonToColor(fcol, "flashed_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::reloading_team = JsonToColor(fcol, "reloading_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::reloading_enemy = JsonToColor(fcol, "reloading_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::defusing_team = JsonToColor(fcol, "defusing_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::defusing_enemy = JsonToColor(fcol, "defusing_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::scoped_team = JsonToColor(fcol, "scoped_team", { 1.f, 1.f, 1.f, 0.5f });
		cfg::esp::colors::flags::scoped_enemy = JsonToColor(fcol, "scoped_enemy", { 1.f, 1.f, 1.f, 0.8f });

		cfg::esp::colors::flags::c4_team = JsonToColor(fcol, "c4_team", { 1.f, 0.84f, 0.f, 1.f });
		cfg::esp::colors::flags::c4_enemy = JsonToColor(fcol, "c4_enemy", { 1.f, 0.84f, 0.f, 1.f });

		// world
		// spectator list
		cfg::world::spectators::enabled = data["world"]["spectators"].value("enabled", true);
		cfg::world::spectators::detailed = data["world"]["spectators"].value("detailed", false);
		cfg::world::spectators::self_only = data["world"]["spectators"].value("self_only", true);
		cfg::world::spectators::pos = JsonToVec2(data["world"]["spectators"], "pos", {10.f, 100.f});

		// bomb
		cfg::world::bomb::location = data["world"]["bomb"].value("location", true);
		cfg::world::bomb::timer = data["world"]["bomb"].value("timer", true);
		cfg::world::bomb::pos = JsonToVec2(data["world"]["bomb"], "pos", { 10.f, 300.f });

		// crosshair
		const auto& crosshair = data["world"]["crosshair"];
		cfg::world::crosshair::enabled = crosshair.value("enabled", false);
		cfg::world::crosshair::sniper_only = crosshair.value("sniper_only", true);
		cfg::world::crosshair::center_dot = crosshair.value("center_dot", false);
		cfg::world::crosshair::outline = crosshair.value("outline", true);
		cfg::world::crosshair::gap = crosshair.value("gap", 6.0f);
		cfg::world::crosshair::length = crosshair.value("length", 6.0f);
		cfg::world::crosshair::thickness = crosshair.value("thickness", 1.0f);
		cfg::world::crosshair::center_dot_size = crosshair.value("center_dot_size", 1.5f);
		cfg::world::crosshair::outline_thickness = crosshair.value("outline_thickness", 1.0f);
		cfg::world::crosshair::color = JsonToColor(crosshair, "color", { 1.f, 1.f, 1.f, 1.f });

		// radar
		cfg::world::radar::enabled = data["world"]["radar"].value("enabled", true);
		cfg::world::radar::no_rotate = data["world"]["radar"].value("no_rotate", false);
		cfg::world::radar::range = data["world"]["radar"].value("range", 2000.f);
		cfg::world::radar::pos = JsonToVec2(data["world"]["radar"], "pos", { 10.f, 10.f });
		cfg::world::radar::size = JsonToVec2(data["world"]["radar"], "size", { 200.f, 200.f });

		// velocity
		cfg::world::velocity::enabled = data["world"]["velocity"].value("enabled", false);
		cfg::world::velocity::sample_rate = data["world"]["velocity"].value("sample_rate", 10);
		cfg::world::velocity::sample_length = data["world"]["velocity"].value("sample_length", 5.f);
		cfg::world::velocity::pos = JsonToVec2(data["world"]["velocity"], "pos", { 10.f, 400.f });
		cfg::world::velocity::size = JsonToVec2(data["world"]["velocity"], "size", { 400.f, 100.f });

		// utils
		//cfg::settings::console = data["utils"].value("console", true);
		cfg::settings::watermark = data["utils"].value("watermark", true);
		cfg::settings::streamproof = data["utils"].value("streamproof", true);
		cfg::settings::vsync = data["utils"].value("vsync", true);
		cfg::settings::free_cpu = data["utils"].value("free_cpu", true);
		cfg::settings::panic_key = data["utils"].value("panic_key", true);
		//cfg::settings::open_menu_key = data["utils"].value("open_menu_key", 0);

		// macro (guard: older configs may not have the section, and operator[] on
		// a missing key creates null, which .value() rejects)
		if (data.contains("macro")) {
			cfg::macro::awp_quickswitch = data["macro"].value("awp_quickswitch", false);
			cfg::macro::delay_ms = data["macro"].value("delay_ms", 100);
			cfg::macro::auto_switch_all = data["macro"].value("auto_switch_all", true);
		}

		// aim (kernel mouse aiming)
		if (data.contains("aim")) {
			const auto& aim = data["aim"];
			cfg::aim::enabled = aim.value("enabled", false);
			cfg::aim::game_mode = aim.value("game_mode", true);
			cfg::aim::aim_at_enemies = aim.value("aim_at_enemies", true);
			cfg::aim::hotkey = aim.value("hotkey", true);
			cfg::aim::visible_only = aim.value("visible_only", false);
			cfg::aim::auto_start = aim.value("auto_start", false);
			cfg::aim::target_part = aim.value("target_part", 3);
			cfg::aim::rifle_mult = aim.value("rifle_mult", 1.0f);
			cfg::aim::pistol_mult = aim.value("pistol_mult", 1.2f);
			cfg::aim::sniper_mult = aim.value("sniper_mult", 0.7f);
			cfg::aim::smg_mult = aim.value("smg_mult", 1.1f);
			cfg::aim::recoil_compensation = aim.value("recoil_compensation", 0.0f);
			cfg::aim::target_switch_delay = aim.value("target_switch_delay", 0.0f);
			cfg::aim::deadzone = aim.value("deadzone", 1.0f);
			cfg::aim::max_delta = aim.value("max_delta", 24.0f);
			cfg::aim::speed = aim.value("speed", 2600.0f);
			cfg::aim::smoothness = aim.value("smoothness", 0.22f);
			cfg::aim::aim_smoothing = aim.value("aim_smoothing", 0.50f);
			cfg::aim::lead_time = aim.value("lead_time", 0.25f);
			cfg::aim::fov_radius = aim.value("fov_radius", 350.0f);
			cfg::aim::exit_fov_mult = aim.value("exit_fov_mult", 1.6f);
			cfg::aim::toggle_key = aim.value("toggle_key", 292);
			cfg::aim::lock_burst = aim.value("lock_burst", false);
		}

		// bypass
		if (data.contains("bypass")) {
			const auto& bp = data["bypass"];
			cfg::bypass::timing_jitter = bp.value("timing_jitter", true);
			cfg::bypass::humanize_movement = bp.value("humanize_movement", true);
			cfg::bypass::write_delay_min_us = bp.value("write_delay_min_us", 50);
			cfg::bypass::write_delay_max_us = bp.value("write_delay_max_us", 200);
			cfg::bypass::noise_amplitude = bp.value("noise_amplitude", 0.3f);
		}

		// triggerbot
		if (data.contains("triggerbot")) {
			const auto& tb = data["triggerbot"];
			cfg::triggerbot::enabled = tb.value("enabled", false);
			cfg::triggerbot::hotkey = tb.value("hotkey", true);
			cfg::triggerbot::visible_only = tb.value("visible_only", true);
			cfg::triggerbot::delay_ms = tb.value("delay_ms", 0);
			cfg::triggerbot::burst_count = tb.value("burst_count", 1);
			cfg::triggerbot::burst_delay_ms = tb.value("burst_delay_ms", 80);
			cfg::triggerbot::threshold = tb.value("threshold", 12.0f);
			cfg::triggerbot::dwell_ms = tb.value("dwell_ms", 25);
			cfg::triggerbot::pistols_only = tb.value("pistols_only", false);
			cfg::triggerbot::rifles_only = tb.value("rifles_only", false);
		}

		// audio
		if (data.contains("audio")) {
			cfg::audio::lock_sound = data["audio"].value("lock_sound", false);
		}

		// skins — load active skin overrides
		if (data.contains("skins") && data["skins"].is_object()) {
			for (auto& [key, val] : data["skins"].items()) {
				int weapon_id = std::stoi(key);
				auto& skin = SkinChanger::Get(weapon_id);
				skin.paint_kit = val.value("paint_kit", 0);
				skin.wear = val.value("wear", 0.0f);
				skin.seed = val.value("seed", 0);
				skin.stattrak = val.value("stattrak", -1);
			}
			SkinChanger::ForceUpdate();
		}
	}
	catch (const std::exception& e) {
		LOGF(WARNING, "Invalid configuration value ({}); restoring defaults", e.what());
		WriteImpl(path);
		return false;
	}

	LOGF(INFO, "Successfully parsed configuration");
	return true;
}

bool Config::WriteImpl(const std::string& path) {
	EnsureConfigDir();
	std::ofstream f(path);

	json data;

	data["enabled"] = cfg::enabled;

	// esp
	data["esp"]["box"] = cfg::esp::box;
	data["esp"]["box_filled"] = cfg::esp::box_filled;
	data["esp"]["box_fill_alpha"] = cfg::esp::box_fill_alpha;
	data["esp"]["box_thickness"] = cfg::esp::box_thickness;
	data["esp"]["skeleton_thickness"] = cfg::esp::skeleton_thickness;
	data["esp"]["head_tracker_size"] = cfg::esp::head_tracker_size;
	data["esp"]["tracer_thickness"] = cfg::esp::tracer_thickness;
	data["esp"]["team"] = cfg::esp::team;
	data["esp"]["armor"] = cfg::esp::armor;
	data["esp"]["health"] = cfg::esp::health;
	data["esp"]["health_number"] = cfg::esp::health_number;
	data["esp"]["skeleton"] = cfg::esp::skeleton;
	data["esp"]["head_tracker"] = cfg::esp::head_tracker;
	data["esp"]["head_tracker_filled"] = cfg::esp::head_tracker_filled;
	data["esp"]["spotted"] = cfg::esp::spotted;
	data["esp"]["spotted_only"] = cfg::esp::spotted_only;
	data["esp"]["distance"] = cfg::esp::distance;
	data["esp"]["headshot_line"] = cfg::esp::headshot_line;
	data["esp"]["tracers"] = cfg::esp::tracers;
	data["esp"]["bomb"] = cfg::esp::bomb;

	// bullet tracer
	data["esp"]["bullet_tracer"]["enabled"] = cfg::esp::bullet_tracer::enabled;
	data["esp"]["bullet_tracer"]["length"] = cfg::esp::bullet_tracer::length;
	data["esp"]["bullet_tracer"]["duration"] = cfg::esp::bullet_tracer::duration;
	data["esp"]["bullet_tracer"]["muzzle_offset"] = cfg::esp::bullet_tracer::muzzle_offset;
	data["esp"]["bullet_tracer"]["thickness"] = cfg::esp::bullet_tracer::thickness;
	ColorToJson(data["esp"]["bullet_tracer"], "team", cfg::esp::bullet_tracer::team);
	ColorToJson(data["esp"]["bullet_tracer"], "enemy", cfg::esp::bullet_tracer::enemy);

	// flags
	data["esp"]["flags"]["name"] = cfg::esp::flags::name;
	data["esp"]["flags"]["ping"] = cfg::esp::flags::ping;
	data["esp"]["flags"]["money"] = cfg::esp::flags::money;
	data["esp"]["flags"]["scoped"] = cfg::esp::flags::scoped;
	data["esp"]["flags"]["weapon"] = cfg::esp::flags::weapon;
	data["esp"]["flags"]["ammo"] = cfg::esp::flags::ammo;
	data["esp"]["flags"]["reloading"] = cfg::esp::flags::reloading;
	data["esp"]["flags"]["flashed"] = cfg::esp::flags::flashed;
	data["esp"]["flags"]["defusing"] = cfg::esp::flags::defusing;
	data["esp"]["flags"]["has_c4"] = cfg::esp::flags::has_c4;

	// world
	// spectator list
	data["world"]["spectators"]["enabled"] = cfg::world::spectators::enabled;
	data["world"]["spectators"]["detailed"] = cfg::world::spectators::detailed;
	data["world"]["spectators"]["self_only"] = cfg::world::spectators::self_only;
	Vec2ToJson(data["world"]["spectators"], "pos", cfg::world::spectators::pos);

	// bomb
	data["world"]["bomb"]["location"] = cfg::world::bomb::location;
	data["world"]["bomb"]["timer"] = cfg::world::bomb::timer;
	Vec2ToJson(data["world"]["bomb"], "pos", cfg::world::bomb::pos);

	// crosshair
	data["world"]["crosshair"]["enabled"] = cfg::world::crosshair::enabled;
	data["world"]["crosshair"]["sniper_only"] = cfg::world::crosshair::sniper_only;
	data["world"]["crosshair"]["center_dot"] = cfg::world::crosshair::center_dot;
	data["world"]["crosshair"]["outline"] = cfg::world::crosshair::outline;
	data["world"]["crosshair"]["gap"] = cfg::world::crosshair::gap;
	data["world"]["crosshair"]["length"] = cfg::world::crosshair::length;
	data["world"]["crosshair"]["thickness"] = cfg::world::crosshair::thickness;
	data["world"]["crosshair"]["center_dot_size"] = cfg::world::crosshair::center_dot_size;
	data["world"]["crosshair"]["outline_thickness"] = cfg::world::crosshair::outline_thickness;
	ColorToJson(data["world"]["crosshair"], "color", cfg::world::crosshair::color);

	// radar
	data["world"]["radar"]["enabled"] = cfg::world::radar::enabled;
	data["world"]["radar"]["no_rotate"] = cfg::world::radar::no_rotate;
	data["world"]["radar"]["range"] = cfg::world::radar::range;
	Vec2ToJson(data["world"]["radar"], "pos", cfg::world::radar::pos);
	Vec2ToJson(data["world"]["radar"], "size", cfg::world::radar::size);

	// velocity
	data["world"]["velocity"]["enabled"] = cfg::world::velocity::enabled;
	data["world"]["velocity"]["sample_rate"] = cfg::world::velocity::sample_rate;
	data["world"]["velocity"]["sample_length"] = cfg::world::velocity::sample_length;
	Vec2ToJson(data["world"]["velocity"], "pos", cfg::world::velocity::pos);
	Vec2ToJson(data["world"]["velocity"], "size", cfg::world::velocity::size);

	// colors
	auto& col = data["esp"]["colors"];
	ColorToJson(col, "box_team", cfg::esp::colors::box_team);
	ColorToJson(col, "box_enemy", cfg::esp::colors::box_enemy);

	ColorToJson(col, "skeleton_team", cfg::esp::colors::skeleton_team);
	ColorToJson(col, "skeleton_enemy", cfg::esp::colors::skeleton_enemy);

	ColorToJson(col, "tracker_team", cfg::esp::colors::tracker_team);
	ColorToJson(col, "tracker_enemy", cfg::esp::colors::tracker_enemy);

	ColorToJson(col, "tracer_team", cfg::esp::colors::tracer_team);
	ColorToJson(col, "tracer_enemy", cfg::esp::colors::tracer_enemy);

	ColorToJson(col, "bomb", cfg::esp::colors::bomb);

	// flag colors
	auto& fcol = col["flags"];

	ColorToJson(fcol, "blinded_team", cfg::esp::colors::flags::flashed_team);
	ColorToJson(fcol, "blinded_enemy", cfg::esp::colors::flags::flashed_enemy);

	ColorToJson(fcol, "reloading_team", cfg::esp::colors::flags::reloading_team);
	ColorToJson(fcol, "reloading_enemy", cfg::esp::colors::flags::reloading_enemy);

	ColorToJson(fcol, "defusing_team", cfg::esp::colors::flags::defusing_team);
	ColorToJson(fcol, "defusing_enemy", cfg::esp::colors::flags::defusing_enemy);

	ColorToJson(fcol, "scoped_team", cfg::esp::colors::flags::scoped_team);
	ColorToJson(fcol, "scoped_enemy", cfg::esp::colors::flags::scoped_enemy);

	ColorToJson(fcol, "c4_team", cfg::esp::colors::flags::c4_team);
	ColorToJson(fcol, "c4_enemy", cfg::esp::colors::flags::c4_enemy);

	// utils
	//data["utils"]["console"] = cfg::settings::console;
	data["utils"]["watermark"] = cfg::settings::watermark;
	data["utils"]["streamproof"] = cfg::settings::streamproof;
	data["utils"]["vsync"] = cfg::settings::vsync;
	data["utils"]["free_cpu"] = cfg::settings::free_cpu;
	data["utils"]["panic_key"] = cfg::settings::panic_key;
	//data["utils"]["open_menu_key"] = cfg::settings::open_menu_key;

	// macro
	data["macro"]["awp_quickswitch"] = cfg::macro::awp_quickswitch;
	data["macro"]["delay_ms"] = cfg::macro::delay_ms;
	data["macro"]["auto_switch_all"] = cfg::macro::auto_switch_all;

	// aim (kernel mouse aiming)
	data["aim"]["enabled"] = cfg::aim::enabled;
	data["aim"]["game_mode"] = cfg::aim::game_mode;
	data["aim"]["aim_at_enemies"] = cfg::aim::aim_at_enemies;
	data["aim"]["deadzone"] = cfg::aim::deadzone;
	data["aim"]["max_delta"] = cfg::aim::max_delta;
	data["aim"]["speed"] = cfg::aim::speed;
	data["aim"]["hotkey"] = cfg::aim::hotkey;
	data["aim"]["visible_only"] = cfg::aim::visible_only;
	data["aim"]["auto_start"] = cfg::aim::auto_start;
	data["aim"]["target_part"] = cfg::aim::target_part;
	data["aim"]["rifle_mult"] = cfg::aim::rifle_mult;
	data["aim"]["pistol_mult"] = cfg::aim::pistol_mult;
	data["aim"]["sniper_mult"] = cfg::aim::sniper_mult;
	data["aim"]["smg_mult"] = cfg::aim::smg_mult;
	data["aim"]["recoil_compensation"] = cfg::aim::recoil_compensation;
	data["aim"]["target_switch_delay"] = cfg::aim::target_switch_delay;
	data["aim"]["fov_radius"] = cfg::aim::fov_radius;
	data["aim"]["smoothness"] = cfg::aim::smoothness;
	data["aim"]["aim_smoothing"] = cfg::aim::aim_smoothing;
	data["aim"]["lead_time"] = cfg::aim::lead_time;
	data["aim"]["exit_fov_mult"] = cfg::aim::exit_fov_mult;
	data["aim"]["toggle_key"] = cfg::aim::toggle_key;
	data["aim"]["lock_burst"] = cfg::aim::lock_burst;

	// bypass
	data["bypass"]["timing_jitter"] = cfg::bypass::timing_jitter;
	data["bypass"]["humanize_movement"] = cfg::bypass::humanize_movement;
	data["bypass"]["write_delay_min_us"] = cfg::bypass::write_delay_min_us;
	data["bypass"]["write_delay_max_us"] = cfg::bypass::write_delay_max_us;
	data["bypass"]["noise_amplitude"] = cfg::bypass::noise_amplitude;

	// triggerbot
	data["triggerbot"]["enabled"] = cfg::triggerbot::enabled;
	data["triggerbot"]["hotkey"] = cfg::triggerbot::hotkey;
	data["triggerbot"]["visible_only"] = cfg::triggerbot::visible_only;
	data["triggerbot"]["delay_ms"] = cfg::triggerbot::delay_ms;
	data["triggerbot"]["burst_count"] = cfg::triggerbot::burst_count;
	data["triggerbot"]["burst_delay_ms"] = cfg::triggerbot::burst_delay_ms;
	data["triggerbot"]["threshold"] = cfg::triggerbot::threshold;
	data["triggerbot"]["dwell_ms"] = cfg::triggerbot::dwell_ms;
	data["triggerbot"]["pistols_only"] = cfg::triggerbot::pistols_only;
	data["triggerbot"]["rifles_only"] = cfg::triggerbot::rifles_only;

	// audio
	data["audio"]["lock_sound"] = cfg::audio::lock_sound;

	// skins — save all active skin overrides
	{
		json skins_obj = json::object();
		for (const auto& [id, skin] : SkinChanger::GetAll()) {
			if (skin.paint_kit <= 0) continue;
			json s;
			s["paint_kit"] = skin.paint_kit;
			s["wear"] = skin.wear;
			s["seed"] = skin.seed;
			s["stattrak"] = skin.stattrak;
			skins_obj[std::to_string(id)] = s;
		}
		data["skins"] = skins_obj;
	}

	f << std::setw(4) << data << std::endl;
	f.close();

	LOGF(VERBOSE, "Writing configuration to file");

	return true;
}


// TODO: Refactor this
color_t Config::JsonToColor(const json& parent, const std::string& key, const color_t& def) {
	if (!parent.contains(key) || !parent[key].is_array() || parent[key].size() != 4)
		return def;
	return color_t(
		parent[key][0].get<float>(),
		parent[key][1].get<float>(),
		parent[key][2].get<float>(),
		parent[key][3].get<float>()
	);
}

void Config::ColorToJson(json& parent, const std::string& key, const color_t& color) {
	parent[key] = { color.r, color.g, color.b, color.a };
}

Vec2_t Config::JsonToVec2(const json& parent, const std::string& key, const Vec2_t& def)
{
	if (!parent.contains(key) || !parent[key].is_array() || parent[key].size() != 2)
		return def;

	return Vec2_t{
		parent[key][0].get<float>(),
		parent[key][1].get<float>()
	};
}

void Config::Vec2ToJson(json& parent, const std::string& key, const Vec2_t& vec)
{
	parent[key] = { vec.x, vec.y };
}
