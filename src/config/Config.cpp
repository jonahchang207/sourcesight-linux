#include "Config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <system_error>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

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
		std::ifstream legacy("config.json", std::ios::binary);
		const std::string contents((std::istreambuf_iterator<char>(legacy)), {});
		if (legacy.good() || legacy.eof()) {
			if (AtomicWrite(active_path, contents, false))
				LOGF(INFO, "Imported legacy config.json into profile '{}'", active);
		} else {
			SetError(ErrorCode::ReadFailed, "The legacy configuration could not be imported.");
		}
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
	if (clean.empty()) { SetError(ErrorCode::InvalidProfileName, "Choose a valid profile name."); return; }
	EnsureConfigDir();
	json meta;
	meta["active"] = clean;
	if (AtomicWrite(MetaPath(), meta.dump(2) + "\n", true))
		SetError(ErrorCode::None, "");
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
	std::lock_guard lock(Mutex());
	const std::string clean = SanitizeName(name);
	if (clean.empty()) {
		SetError(ErrorCode::InvalidProfileName, "Choose a valid profile name.");
		LOGF(WARNING, "Invalid profile name, refusing to load");
		return false;
	}
	const bool ok = GetInstance().ReadImpl(ProfilePath(clean));
	if (ok) SetActiveProfile(clean);
	LOGF(INFO, "{} profile '{}'", ok ? "Loaded" : "Did not load", clean);
	return ok;
}

bool Config::SaveProfile(const std::string& name) {
	std::lock_guard lock(Mutex());
	const std::string clean = SanitizeName(name);
	if (clean.empty()) {
		SetError(ErrorCode::InvalidProfileName, "Choose a valid profile name.");
		LOGF(WARNING, "Invalid profile name, refusing to save");
		return false;
	}
	EnsureConfigDir();
	const bool ok = GetInstance().WriteImpl(ProfilePath(clean));
	if (ok) SetActiveProfile(clean);
	LOGF(INFO, "{} profile '{}'", ok ? "Saved" : "Did not save", clean);
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

namespace {
Config::Error& LastConfigError() {
    static Config::Error error;
    return error;
}

std::mutex& ConfigErrorMutex() {
    static std::mutex mutex;
    return mutex;
}

bool& FailWritesForTesting() {
    static bool fail = false;
    return fail;
}

bool CompatibleValue(const json& actual, const json& expected) {
    if (expected.is_object()) return actual.is_object();
    if (expected.is_array()) {
        if (!actual.is_array() || actual.size() != expected.size()) return false;
        for (std::size_t i = 0; i < actual.size(); ++i)
            if (!CompatibleValue(actual[i], expected[i])) return false;
        return true;
    }
    if (expected.is_number_integer() || expected.is_number_unsigned())
        return actual.is_number_integer() && actual >= std::numeric_limits<int>::min() && actual <= std::numeric_limits<int>::max();
    if (expected.is_number_float()) {
        if (!actual.is_number()) return false;
        const double number = actual.get<double>();
        return std::isfinite(number) && number >= -std::numeric_limits<float>::max() && number <= std::numeric_limits<float>::max();
    }
    return actual.type() == expected.type();
}

bool ValidateKnownValues(json& actual, const json& schema, std::string& error) {
    if (!actual.is_object() || !schema.is_object()) return true;
    for (auto it = schema.begin(); it != schema.end(); ++it) {
        if (!actual.contains(it.key())) {
            if (it.value().is_object()) actual[it.key()] = json::object();
            else continue;
        }
        auto& value = actual[it.key()];
        if (it.value().is_object()) {
            if (!value.is_object()) { error = "A configuration section has an invalid shape."; return false; }
            if (!ValidateKnownValues(value, it.value(), error)) return false;
        } else if (!CompatibleValue(value, it.value())) {
            error = "A configuration value has an invalid type or boundary.";
            return false;
        }
    }
    return true;
}

bool WriteDurableTemporary(const std::filesystem::path& parent, const std::string& stem,
                           const std::string& content, std::filesystem::path& output) {
    std::string pattern = (parent / (stem + ".tmp-XXXXXX")).string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    const int fd = mkstemp(writable.data());
    if (fd < 0) return false;
    output = writable.data();
    std::size_t written = 0;
    while (written < content.size()) {
        const auto count = ::write(fd, content.data() + written, content.size() - written);
        if (count <= 0) {
            ::close(fd);
            std::error_code ec;
            std::filesystem::remove(output, ec);
            return false;
        }
        written += static_cast<std::size_t>(count);
    }
    const bool synced = ::fsync(fd) == 0;
    const bool closed = ::close(fd) == 0;
    const bool ok = synced && closed;
    if (!ok) {
        std::error_code ec;
        std::filesystem::remove(output, ec);
    }
    return ok;
}

void SyncDirectory(const std::filesystem::path& directory) {
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        (void)::fsync(fd);
        (void)::close(fd);
    }
}
} // namespace

Config::Error Config::LastError() {
    std::lock_guard lock(ConfigErrorMutex());
    return LastConfigError();
}

const char* Config::ErrorCodeName(ErrorCode code) {
    switch (code) {
    case ErrorCode::None: return "none";
    case ErrorCode::InvalidProfileName: return "invalid_profile_name";
    case ErrorCode::MissingProfile: return "missing_profile";
    case ErrorCode::ParseFailed: return "parse_failed";
    case ErrorCode::InvalidShape: return "invalid_shape";
    case ErrorCode::FutureSchema: return "future_schema";
    case ErrorCode::MigrationFailed: return "migration_failed";
    case ErrorCode::ReadFailed: return "read_failed";
    case ErrorCode::WriteFailed: return "write_failed";
    }
    return "unknown";
}

void Config::SetWriteFailureForTesting(bool fail) {
    std::lock_guard lock(Mutex());
    FailWritesForTesting() = fail;
}

bool Config::ShouldFailWritesForTesting() { return FailWritesForTesting(); }

void Config::SetError(ErrorCode code, std::string message) {
    std::lock_guard lock(ConfigErrorMutex());
    LastConfigError() = {code, std::move(message)};
}

bool Config::ValidateAndNormalize(json& data, ErrorCode& code, std::string& error, bool& migrated) {
	code = ErrorCode::InvalidShape;
    if (!data.is_object()) {
        error = "The profile root must be an object.";
        return false;
    }
    std::uint64_t version = 0;
    if (data.contains("schema_version")) {
        if (!data["schema_version"].is_number_integer()) {
            error = "The profile schema version is invalid.";
            return false;
        }
        if (data["schema_version"].is_number_unsigned())
            version = data["schema_version"].get<std::uint64_t>();
        else {
            const auto signed_version = data["schema_version"].get<std::int64_t>();
            if (signed_version < 0) {
                error = "The profile schema version is invalid.";
                return false;
            }
            version = static_cast<std::uint64_t>(signed_version);
        }
    }
    if (version > static_cast<std::uint64_t>(SchemaVersion())) {
		code = ErrorCode::FutureSchema;
        error = "This profile was created by a newer version and was not changed.";
        return false;
    }

    // Version 0 was the historical unversioned flat/nested profile. Version 1
    // introduced profiles but no semantic field changes. Both migrate by
    // adding the explicit current version while retaining unknown fields.
    migrated = version != static_cast<std::uint64_t>(SchemaVersion());
    data["schema_version"] = SchemaVersion();
    const json schema = BuildCurrentJson(json::object());
    return ValidateKnownValues(data, schema, error);
}

bool Config::AtomicWrite(const std::string& path, const std::string& content, bool backup_existing) {
    if (ShouldFailWritesForTesting()) {
        SetError(ErrorCode::WriteFailed, "The profile could not be written; the previous file was kept.");
        return false;
    }
    const std::filesystem::path target(path);
    std::error_code ec;
    if (std::filesystem::is_symlink(target, ec)) {
        SetError(ErrorCode::WriteFailed, "The profile destination is unsafe.");
        return false;
    }
    const auto parent = target.parent_path();
    if (!std::filesystem::exists(parent, ec) || ec) {
        SetError(ErrorCode::WriteFailed, "The profile directory is unavailable.");
        return false;
    }

    std::filesystem::path temporary;
    if (!WriteDurableTemporary(parent, target.filename().string(), content, temporary)) {
        SetError(ErrorCode::WriteFailed, "The profile could not be written; the previous file was kept.");
        return false;
    }

    const bool exists = std::filesystem::exists(target, ec) && !ec;
    if (backup_existing && exists) {
        const auto backup = parent / (target.filename().string() + ".bak");
        std::ifstream input(target, std::ios::binary);
        const std::string previous((std::istreambuf_iterator<char>(input)), {});
        std::filesystem::path backup_temp;
        if ((!input.good() && !input.eof()) || !WriteDurableTemporary(parent, backup.filename().string(), previous, backup_temp)) {
            std::filesystem::remove(temporary, ec);
            SetError(ErrorCode::WriteFailed, "The profile backup could not be created; the previous file was kept.");
            return false;
        }
        std::filesystem::rename(backup_temp, backup, ec);
        if (ec) {
            std::filesystem::remove(backup_temp, ec);
            std::filesystem::remove(temporary, ec);
            SetError(ErrorCode::WriteFailed, "The profile backup could not be created; the previous file was kept.");
            return false;
        }
		SyncDirectory(parent);
    }
    std::filesystem::rename(temporary, target, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        SetError(ErrorCode::WriteFailed, "The profile could not be replaced; the previous file was kept.");
        return false;
    }
    SyncDirectory(parent);
    return true;
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
		if (WriteImpl(path)) {
			SetError(ErrorCode::None, "");
			LOGF(INFO, "Configuration file does not exist; created defaults");
			return true;
		}
		return false;
	}

	json data;
	try {
		data = json::parse(f);
	}
	catch (const std::exception& e) {
		SetError(ErrorCode::ParseFailed, "The profile is malformed; the previous settings were kept.");
		LOGF(WARNING, "Failed to parse configuration file ({}); keeping active settings", e.what());
		return false;
	}

	bool migrated = false;
	std::string validation_error;
	ErrorCode validation_code;
	if (!ValidateAndNormalize(data, validation_code, validation_error, migrated)) {
		SetError(validation_code, validation_error);
		LOGF(WARNING, "Refusing configuration profile: {}", validation_error);
		return false;
	}

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
		{
			const auto object=data["esp"].value("bullet_tracer",json::object());
			const auto bt=object.is_object()?object:json::object();
			cfg::esp::bullet_tracer::enabled = bt.value("enabled", false);
			cfg::esp::bullet_tracer::length = std::clamp(bt.value("length", 8192.0f),50.f,16384.f);
			cfg::esp::bullet_tracer::duration = std::clamp(bt.value("duration", 1.25f),.1f,10.f);
			cfg::esp::bullet_tracer::muzzle_offset = bt.value("muzzle_offset", 45.0f);
			cfg::esp::bullet_tracer::thickness = bt.value("thickness", 1.5f);
			cfg::esp::bullet_tracer::style = std::clamp(bt.value("style",0),0,2);
			cfg::esp::bullet_tracer::glow = std::clamp(bt.value("glow",.65f),0.f,1.f);
			cfg::esp::bullet_tracer::impact = bt.value("impact",true);
			cfg::esp::bullet_tracer::team = JsonToColor(bt, "team", { 0.f, 1.f, 0.5f, 0.6f });
			cfg::esp::bullet_tracer::enemy = JsonToColor(bt, "enemy", { 1.f, 0.3f, 0.3f, 0.6f });
		}
		
		// wireframe
		{
			namespace pw = cfg::esp::player_wireframe;
			const auto object = data["esp"].value("player_wireframe", nlohmann::json::object());
			const auto settings = object.is_object() ? object : nlohmann::json::object();
			pw::enabled = settings.value("enabled", false);
			pw::visible_only = settings.value("visible_only", false);
			pw::detail = std::clamp(settings.value("detail", 1), 0, 2);
			pw::opacity = std::clamp(settings.value("opacity", .8f), 0.f, 1.f);
			pw::thickness = std::clamp(settings.value("thickness", 1.f), 1.f, 3.f);
			pw::max_distance = std::clamp(settings.value("max_distance", 3000.f), 100.f, 10000.f);
			pw::visible = JsonToColor(settings, "visible", {.65f,.86f,.72f,1.f});
			pw::blocked = JsonToColor(settings, "blocked", {.85f,.48f,.42f,1.f});
			pw::unknown = JsonToColor(settings, "unknown", {.55f,.58f,.62f,1.f});
		}
		cfg::esp::wireframe = data["esp"].value("wireframe", false);
		cfg::esp::wireframe_mode = std::clamp(data["esp"].value("wireframe_mode", 0), 0, 1);
		cfg::esp::wireframe_full_xray = data["esp"].value("wireframe_full_xray", false);
		cfg::esp::wireframe_panel_opacity = std::clamp(data["esp"].value("wireframe_panel_opacity", .10f), 0.f, .35f);
		// Ignore legacy wireframe_occlude_game: its forced opaque fill hid the game.
		cfg::esp::wireframe_max_dist = data["esp"].value("wireframe_max_dist", 3000.0f);
		cfg::esp::wireframe_budget = std::clamp(data["esp"].value("wireframe_budget", 6000), 500, 8000);
		cfg::esp::wireframe_opacity = std::clamp(data["esp"].value("wireframe_opacity", 0.65f), 0.0f, 1.0f);
		cfg::esp::wireframe_color = JsonToColor(data["esp"], "wireframe_color", {100.f/255.f, 215.f/255.f, 220.f/255.f, 1.f});
		
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
		cfg::world::radar::minimap = data["world"]["radar"].value("minimap", true);
		cfg::world::radar::auto_sync = data["world"]["radar"].value("auto_sync", true);
		cfg::world::radar::zoom = std::clamp(data["world"]["radar"].value("zoom", .7f), .25f, 1.f);
		cfg::world::radar::hud_scale = std::clamp(data["world"]["radar"].value("hud_scale", 1.f), .5f, 2.f);
		cfg::world::radar::hud_size = std::clamp(data["world"]["radar"].value("hud_size", 1.f), .5f, 2.f);
		cfg::world::radar::calibration_height = std::clamp(data["world"]["radar"].value("calibration_height", 0.f), 0.f, 16384.f);
		cfg::world::radar::scale_correction = std::clamp(data["world"]["radar"].value("scale_correction", 1.f), .75f, 1.25f);
		cfg::world::radar::offset = JsonToVec2(data["world"]["radar"], "offset", {0.f, 0.f});
		cfg::world::radar::opacity = std::clamp(data["world"]["radar"].value("opacity", 0.20f), 0.0f, 1.0f);
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
			cfg::aim::priority = aim.value("priority", 0);
			cfg::aim::lock_burst = aim.value("lock_burst", false);
		}

// spinbot
		if (data.contains("spinbot")) {
			const auto& sb = data["spinbot"];
			cfg::spinbot::enabled = sb.value("enabled", false);
			cfg::spinbot::shoot = sb.value("shoot", true);
			cfg::spinbot::direction = sb.value("direction", 1);
			cfg::spinbot::speed = sb.value("speed", 1600.0f);
			cfg::spinbot::pitch_sway = sb.value("pitch_sway", 0.0f);
			cfg::spinbot::sway_hz = sb.value("sway_hz", 2.5f);
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

		// sound_esp
		if (data.contains("sound_esp")) {
			const auto& se = data["sound_esp"];
			cfg::sound_esp::enabled = se.value("enabled", false);
			cfg::sound_esp::footsteps = se.value("footsteps", true);
			cfg::sound_esp::gunshots = se.value("gunshots", true);
			cfg::sound_esp::max_distance = se.value("max_distance", 1000.0f);
			cfg::sound_esp::duration = se.value("duration", 3.0f);
			cfg::sound_esp::fade_time = se.value("fade_time", 1.0f);
			cfg::sound_esp::footprint_size = se.value("footprint_size", 8.0f);
			cfg::sound_esp::footsteps_color = JsonToColor(se, "footsteps_color", { 1.f, 1.f, 1.f, 0.8f });
			cfg::sound_esp::gunshots_color = JsonToColor(se, "gunshots_color", { 1.f, 0.3f, 0.3f, 0.9f });
		}

		// capture
		if (data.contains("capture")) {
			const auto& cap = data["capture"];
			cfg::capture::fps = cap.value("fps", 60);
			if (cfg::capture::fps < 1)
				cfg::capture::fps = 1;
			if (cfg::capture::fps > 240)
				cfg::capture::fps = 240;
			cfg::capture::output_dir = cap.value("output_dir", std::string("captures"));
			if (cfg::capture::output_dir.empty())
				cfg::capture::output_dir = "captures";
		}

	}
	catch (const std::exception& e) {
		SetError(ErrorCode::InvalidShape, "A configuration value could not be applied; active settings were kept.");
		LOGF(WARNING, "Invalid configuration value ({}); keeping active settings", e.what());
		return false;
	}

	if (migrated && !WriteImpl(path))
		return false;
	SetError(ErrorCode::None, "");
	LOGF(INFO, "Successfully parsed configuration");
	return true;
}

json Config::BuildCurrentJson(json data) {
	data["schema_version"] = SchemaVersion();

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
	data["esp"]["bullet_tracer"]["style"] = cfg::esp::bullet_tracer::style;
	data["esp"]["bullet_tracer"]["glow"] = cfg::esp::bullet_tracer::glow;
	data["esp"]["bullet_tracer"]["impact"] = cfg::esp::bullet_tracer::impact;
	ColorToJson(data["esp"]["bullet_tracer"], "team", cfg::esp::bullet_tracer::team);
	ColorToJson(data["esp"]["bullet_tracer"], "enemy", cfg::esp::bullet_tracer::enemy);

	// wireframe
	{
		namespace pw = cfg::esp::player_wireframe;
		auto& settings = data["esp"]["player_wireframe"];
		settings["enabled"] = pw::enabled;
		settings["visible_only"] = pw::visible_only;
		settings["detail"] = pw::detail;
		settings["opacity"] = pw::opacity;
		settings["thickness"] = pw::thickness;
		settings["max_distance"] = pw::max_distance;
		ColorToJson(settings, "visible", pw::visible);
		ColorToJson(settings, "blocked", pw::blocked);
		ColorToJson(settings, "unknown", pw::unknown);
	}
	data["esp"]["wireframe"] = cfg::esp::wireframe;
	data["esp"]["wireframe_mode"] = cfg::esp::wireframe_mode;
	data["esp"]["wireframe_full_xray"] = cfg::esp::wireframe_full_xray;
	data["esp"]["wireframe_panel_opacity"] = cfg::esp::wireframe_panel_opacity;
	data["esp"]["wireframe_max_dist"] = cfg::esp::wireframe_max_dist;
	data["esp"]["wireframe_budget"] = cfg::esp::wireframe_budget;
	data["esp"]["wireframe_opacity"] = cfg::esp::wireframe_opacity;
	ColorToJson(data["esp"], "wireframe_color", cfg::esp::wireframe_color);
	
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
	data["world"]["radar"]["minimap"] = cfg::world::radar::minimap;
	data["world"]["radar"]["auto_sync"] = cfg::world::radar::auto_sync;
	data["world"]["radar"]["zoom"] = cfg::world::radar::zoom;
	data["world"]["radar"]["hud_scale"] = cfg::world::radar::hud_scale;
	data["world"]["radar"]["hud_size"] = cfg::world::radar::hud_size;
	data["world"]["radar"]["calibration_height"] = cfg::world::radar::calibration_height;
	data["world"]["radar"]["scale_correction"] = cfg::world::radar::scale_correction;
	Vec2ToJson(data["world"]["radar"], "offset", cfg::world::radar::offset);
	data["world"]["radar"]["opacity"] = cfg::world::radar::opacity;
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
	data["aim"]["priority"] = cfg::aim::priority;
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

	// spinbot
	data["spinbot"]["enabled"] = cfg::spinbot::enabled;
	data["spinbot"]["shoot"] = cfg::spinbot::shoot;
	data["spinbot"]["direction"] = cfg::spinbot::direction;
	data["spinbot"]["speed"] = cfg::spinbot::speed;
	data["spinbot"]["pitch_sway"] = cfg::spinbot::pitch_sway;
	data["spinbot"]["sway_hz"] = cfg::spinbot::sway_hz;

	// audio
	data["audio"]["lock_sound"] = cfg::audio::lock_sound;

	// sound_esp
	data["sound_esp"]["enabled"] = cfg::sound_esp::enabled;
	data["sound_esp"]["footsteps"] = cfg::sound_esp::footsteps;
	data["sound_esp"]["gunshots"] = cfg::sound_esp::gunshots;
	data["sound_esp"]["max_distance"] = cfg::sound_esp::max_distance;
	data["sound_esp"]["duration"] = cfg::sound_esp::duration;
	data["sound_esp"]["fade_time"] = cfg::sound_esp::fade_time;
	data["sound_esp"]["footprint_size"] = cfg::sound_esp::footprint_size;
	ColorToJson(data["sound_esp"], "footsteps_color", cfg::sound_esp::footsteps_color);
	ColorToJson(data["sound_esp"], "gunshots_color", cfg::sound_esp::gunshots_color);

	// capture
	data["capture"]["fps"] = cfg::capture::fps;
	data["capture"]["output_dir"] = cfg::capture::output_dir;

	// Retire the old opaque map-fill setting permanently.  It is intentionally
	// ignored on load and must not survive a save through unknown-field merging.
	if (data.contains("esp") && data["esp"].is_object())
		data["esp"].erase("wireframe_occlude_game");
	return data;
}

bool Config::WriteImpl(const std::string& path) {
	if (!EnsureConfigDir()) {
		SetError(ErrorCode::WriteFailed, "The profile directory could not be created.");
		return false;
	}

	json data = json::object();
	std::ifstream existing(path, std::ios::binary);
	if (existing.good()) {
		try {
			data = json::parse(existing);
			bool ignored_migration = false;
			ErrorCode validation_code;
			std::string error;
			if (!ValidateAndNormalize(data, validation_code, error, ignored_migration)) {
				SetError(ErrorCode::WriteFailed, "The existing profile is invalid and was not overwritten.");
				return false;
			}
		}
		catch (const std::exception&) {
			SetError(ErrorCode::WriteFailed, "The existing profile is malformed and was not overwritten.");
			return false;
		}
	}

	data = BuildCurrentJson(std::move(data));
	const bool ok = AtomicWrite(path, data.dump(4) + "\n", true);
	if (ok) {
		SetError(ErrorCode::None, "");
		LOGF(VERBOSE, "Writing configuration to file");
	}
	return ok;
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
