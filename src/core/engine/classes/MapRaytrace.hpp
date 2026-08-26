#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

// Robust external visibility check for CS2 using map collision geometry.
//
// How it works:
//   1. At startup (or on map change), the system reads the current map name
//      from CS2's GlobalVars and loads the corresponding .tri file.
//   2. The .tri file contains the map's collision mesh triangles (each triangle
//      = 3 × Vec3 floats = 36 bytes). These are extracted offline from the
//      map's .vpk using Source 2 Viewer → decompress world_physics.vphys_c →
//      convert to .tri with the cs2-map-parser tool.
//   3. A KD-tree is built from the triangles for fast spatial queries.
//   4. Each visibility test fires a ray from eye position to target position
//      and checks if any triangle blocks it.
//
// .tri file placement:
//   Put the .tri files in a "maps/" folder next to the executable, named
//   after the map (e.g. maps/de_dust2.tri, maps/de_mirage.tri).
//
// Credits: Based on AtomicBool/cs2-map-parser and Read1dno/VisCheckCS2.

namespace MapRaytrace {

struct Vec3 {
    float x, y, z;
};

struct Triangle {
    Vec3 p1, p2, p3;
};

struct AABB {
    Vec3 min, max;
};

// KD-tree node for accelerated raycasting.
struct KDNode {
    AABB bbox;
    std::vector<uint32_t> triangle_indices;  // indices into global tri list (leaf only)
    KDNode* left = nullptr;
    KDNode* right = nullptr;
};

// Initialize the raytrace system. Call once at startup.
// map_folder: directory containing .tri files (default: "maps")
void Init(const std::string& map_folder = "maps");

// Load a map's .tri file and build the KD-tree.
// Returns true on success. Safe to call multiple times (reloads).
bool LoadMap(const std::string& map_name);

// Unload the current map and free memory.
void Unload();

// Check if a ray from `origin` to `target` is blocked by map geometry.
// Returns true if the path is CLEAR (visible), false if blocked.
bool IsVisible(const Vec3& origin, const Vec3& target);

// Get the name of the currently loaded map (empty if none).
const std::string& CurrentMap();

// True when a valid map is loaded and ready for raycasting.
bool IsReady();

} // namespace MapRaytrace
