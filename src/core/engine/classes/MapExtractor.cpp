#include "MapExtractor.hpp"
#include "MapRaytrace.hpp"

#include "core/engine/Engine.hpp"
#include "core/logger/LogHelper.hpp"

#include <fstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

namespace MapExtractor {

namespace {
    std::string g_cs2_install_path;
    std::string g_maps_dir = "maps";
    bool g_initialized = false;
    std::vector<std::string> g_vpk_files;
}

bool Init() {
    if (g_initialized)
        return true;

    // Find CS2 install path
    auto cs2_path = FindCS2InstallPath();
    if (!cs2_path) {
        LOGF(WARNING, "[map_extractor] Could not find CS2 installation");
        return false;
    }
    g_cs2_install_path = *cs2_path;

    // Find all VPK files in cs2/maps/
    std::string maps_vpk_dir = g_cs2_install_path + "/game/csgo/maps";
    if (!std::filesystem::exists(maps_vpk_dir)) {
        maps_vpk_dir = g_cs2_install_path + "/game/csgo/maps";
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(maps_vpk_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vpk") {
            g_vpk_files.push_back(entry.path().string());
        }
    }

    if (g_vpk_files.empty()) {
        LOGF(WARNING, "[map_extractor] No VPK files found in {}", maps_vpk_dir);
    } else {
        LOGF(INFO, "[map_extractor] Found {} VPK files", g_vpk_files.size());
    }

    // Create maps output directory
    std::filesystem::create_directories(g_maps_dir, ec);

    g_initialized = true;
    return true;
}

std::optional<std::string> FindCS2InstallPath() {
    // Method 1: Check Steam registry (Windows) or Steam config (Linux)
    // Method 2: Check common install locations
    // Method 3: Use Steam API if available

    std::vector<std::string> common_paths = {
        // Linux Steam default
        std::string(std::getenv("HOME")) + "/.steam/steam/steamapps/common/Counter-Strike 2",
        std::string(std::getenv("HOME")) + "/.local/share/Steam/steamapps/common/Counter-Strike 2",
        "/mnt/steam/steamapps/common/Counter-Strike 2",
        "/home/steam/steamapps/common/Counter-Strike 2",
        // Windows (if running via Wine/Proton)
        "C:/Program Files (x86)/Steam/steamapps/common/Counter-Strike 2",
        "C:/Program Files/Steam/steamapps/common/Counter-Strike 2",
    };

    // Also check Steam libraryfolders.vdf for additional library paths
    std::vector<std::string> steam_configs = {
        std::string(std::getenv("HOME")) + "/.steam/steam/steamapps/libraryfolders.vdf",
        std::string(std::getenv("HOME")) + "/.local/share/Steam/steamapps/libraryfolders.vdf",
    };

    for (const auto& config : steam_configs) {
        if (std::filesystem::exists(config)) {
            // Parse libraryfolders.vdf for additional paths
            // Simple string search for "path" entries
            std::ifstream in(config);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            
            size_t pos = 0;
            while ((pos = content.find("\"path\"", pos)) != std::string::npos) {
                size_t quote1 = content.find('"', pos + 6);
                size_t quote2 = content.find('"', quote1 + 1);
                if (quote1 != std::string::npos && quote2 != std::string::npos) {
                    std::string lib_path = content.substr(quote1 + 1, quote2 - quote1 - 1);
                    // Fix escaped backslashes
                    std::replace(lib_path.begin(), lib_path.end(), '\\', '/');
                    std::string cs2_path = lib_path + "/steamapps/common/Counter-Strike 2";
                    common_paths.push_back(cs2_path);
                }
                pos = quote2;
            }
        }
    }

    for (const auto& path : common_paths) {
        if (std::filesystem::exists(path)) {
            LOGF(INFO, "[map_extractor] Found CS2 at: {}", path);
            return path;
        }
    }

    return std::nullopt;
}

bool HasMapData(const std::string& map_name, const std::string& maps_dir) {
    std::string tri_path = maps_dir + "/" + map_name + ".tri";
    return std::filesystem::exists(tri_path);
}

std::optional<std::string> GetMapTriPath(const std::string& map_name, const std::string& maps_dir) {
    std::string tri_path = maps_dir + "/" + map_name + ".tri";
    if (std::filesystem::exists(tri_path))
        return tri_path;
    return std::nullopt;
}

ExtractResult ExtractMap(const std::string& map_name, const std::string& output_dir) {
    ExtractResult result;
    
    if (!g_initialized)
        Init();

    // Check if already extracted
    if (HasMapData(map_name, output_dir)) {
        result.success = true;
        result.tri_path = output_dir + "/" + map_name + ".tri";
        return result;
    }

    // Try to find the map in VPK files
    // We need to extract world_physics.vmdl_c from the map's VPK
    // This requires VPK parsing + vmdl_c (KV3) parsing
    
    // For now, try to call external extractor tool if available
    // The C# tool (cs2-phys-extractor) can do this automatically
    
    result.success = false;
    result.error = "Native VPK extraction not yet implemented. "
                   "Place .tri files manually in maps/ folder, "
                   "or use cs2-phys-extractor (C#) to extract from VPKs.";
    
    // TODO: Implement native VPK + vmdl_c extraction:
    // 1. Find map VPK (de_mirage.vpk or pak01_dir.vpk)
    // 2. Extract maps/de_mirage/world_physics.vmdl_c
    // 3. Parse KV3 to get physics mesh data
    // 4. Convert to .tri format (Triangle {Vec3 p1,p2,p3})
    // 5. Save to output_dir/map_name.tri
    
    return result;
}

bool EnsureMapLoaded(const std::string& map_name) {
    if (map_name.empty())
        return false;

    // Check if already loaded in MapRaytrace
    if (MapRaytrace::CurrentMap() == map_name && MapRaytrace::IsReady())
        return true;

    // Check if .tri exists locally
    if (HasMapData(map_name)) {
        return MapRaytrace::LoadMap(map_name);
    }

    // Try to extract
    auto result = ExtractMap(map_name);
    if (result.success) {
        return MapRaytrace::LoadMap(map_name);
    }

    LOGF(WARNING, "[map_extractor] No collision data for map '{}': {}", map_name, result.error);
    return false;
}

std::vector<std::string> ListAvailableMaps() {
    std::vector<std::string> maps;
    
    if (!g_initialized)
        Init();

    // Known official maps
    static const std::vector<std::string> official_maps = {
        "de_dust2", "de_mirage", "de_inferno", "de_nuke", "de_overpass",
        "de_vertigo", "de_ancient", "de_anubis", "de_train", "de_cache",
        "de_cbble", "de_season", "de_aztec", "de_dust", "de_italy",
        "de_office", "de_agency", "de_basalt", "de_thera", "de_mills",
        "de_crown", "de_lake", "de_vostok", "de_klim", "de_mooncamp",
        "de_shattered", "de_canals", "de_shortdust", "de_shortnuke",
        "de_safehouse", "de_olddust2", "de_vietnam", "de_stmarc",
        "de_museum", "de_breach", "de_river", "de_frost", "de_tulip",
        "de_havana", "de_primetime", "de_rubicon", "de_bikini", "de_grotto"
    };

    for (const auto& map : official_maps) {
        if (HasMapData(map))
            maps.push_back(map);
    }

    return maps;
}

} // namespace MapExtractor