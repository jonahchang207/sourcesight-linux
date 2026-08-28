#pragma once

#include "Current.hpp"

#include <mutex>
#include <string>
#include <vector>

using json = nlohmann::json;

// Config profile system.
//
// Each named profile is a JSON file under ./configs/<name>.json. The name of
// the profile that was last loaded/saved is persisted in ./configs/meta.json,
// so the next launch resumes with the same profile. On first run, a legacy
// single-file ./config.json is imported into the "default" profile so nothing
// is lost.
class Config {
public:
    ~Config() = default;
    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;

    // Operate on the active profile.
    static bool Read();
    static bool Write();
    static std::string GetActiveProfile();
    static void SetActiveProfile(const std::string& name);

    // Profile management.
    static std::vector<std::string> ListProfiles();
    static bool HasProfile(const std::string& name);
    static bool LoadProfile(const std::string& name);  // switch active + apply
    static bool SaveProfile(const std::string& name);  // save current cfg + switch active
    static bool DeleteProfile(const std::string& name);
private:
    Config();

    static Config& GetInstance()
    {
        static Config i{};
        return i;
    }

    bool ReadImpl(const std::string& path);
    bool WriteImpl(const std::string& path);

    static std::string ProfileDir();
    static std::string MetaPath();
    static std::string ProfilePath(const std::string& name);
    static std::string SanitizeName(const std::string& name);
    static bool EnsureConfigDir();
    static void EnsureMeta();
    static std::mutex& Mutex();

    static color_t JsonToColor(const json& parent, const std::string& key, const color_t& def);
    static void ColorToJson(json& parent, const std::string& key, const color_t& color);
    static void Vec2ToJson(json& parent, const std::string& key, const Vec2_t& vec);
    static Vec2_t JsonToVec2(const json& parent, const std::string& key, const Vec2_t& def);
};