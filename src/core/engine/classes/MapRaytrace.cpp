#include "MapRaytrace.hpp"
#include "common.hpp"

#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <filesystem>
#include <chrono>

namespace {

constexpr float EPSILON = 1e-6f;
constexpr int   KD_LEAF_THRESHOLD = 8;
constexpr int   KD_MAX_DEPTH = 24;

std::string              g_map_folder = "maps";
std::string              g_current_map;
std::vector<MapRaytrace::Triangle> g_triangles;
MapRaytrace::KDNode*     g_root = nullptr;
std::mutex               g_mutex;
std::atomic<bool>        g_ready{false};

// ── KD-tree helpers ──────────────────────────────────────────────────

MapRaytrace::AABB ComputeBounds(const MapRaytrace::Triangle& t) {
    MapRaytrace::AABB box;
    box.min.x = std::min({t.p1.x, t.p2.x, t.p3.x});
    box.min.y = std::min({t.p1.y, t.p2.y, t.p3.y});
    box.min.z = std::min({t.p1.z, t.p2.z, t.p3.z});
    box.max.x = std::max({t.p1.x, t.p2.x, t.p3.x});
    box.max.y = std::max({t.p1.y, t.p2.y, t.p3.y});
    box.max.z = std::max({t.p1.z, t.p2.z, t.p3.z});
    return box;
}

MapRaytrace::AABB MergeBounds(const MapRaytrace::AABB& a, const MapRaytrace::AABB& b) {
    MapRaytrace::AABB box;
    box.min.x = std::min(a.min.x, b.min.x);
    box.min.y = std::min(a.min.y, b.min.y);
    box.min.z = std::min(a.min.z, b.min.z);
    box.max.x = std::max(a.max.x, b.max.x);
    box.max.y = std::max(a.max.y, b.max.y);
    box.max.z = std::max(a.max.z, b.max.z);
    return box;
}

MapRaytrace::AABB ComputeBoundsFromIndices(const std::vector<MapRaytrace::Triangle>& tris,
                                            const std::vector<uint32_t>& indices) {
    MapRaytrace::AABB box = ComputeBounds(tris[indices[0]]);
    for (size_t i = 1; i < indices.size(); ++i) {
        box = MergeBounds(box, ComputeBounds(tris[indices[i]]));
    }
    return box;
}

MapRaytrace::KDNode* BuildKDTree(const std::vector<MapRaytrace::Triangle>& tris,
                                  std::vector<uint32_t>& indices, int depth) {
    auto* node = new MapRaytrace::KDNode();
    node->bbox = ComputeBoundsFromIndices(tris, indices);

    if ((int)indices.size() <= KD_LEAF_THRESHOLD || depth >= KD_MAX_DEPTH) {
        node->triangle_indices = indices;
        return node;
    }

    // Split along the longest axis of the bounding box
    MapRaytrace::Vec3 extent;
    extent.x = node->bbox.max.x - node->bbox.min.x;
    extent.y = node->bbox.max.y - node->bbox.min.y;
    extent.z = node->bbox.max.z - node->bbox.min.z;

    int axis = 0;
    if (extent.y > extent.x && extent.y > extent.z) axis = 1;
    else if (extent.z > extent.x && extent.z > extent.y) axis = 2;

    // Sort indices by triangle centroid along the chosen axis
    auto comparator = [&](uint32_t a_idx, uint32_t b_idx) {
        const auto& ta = tris[a_idx];
        const auto& tb = tris[b_idx];
        float ca = (axis == 0) ? (ta.p1.x + ta.p2.x + ta.p3.x) / 3.0f
                 : (axis == 1) ? (ta.p1.y + ta.p2.y + ta.p3.y) / 3.0f
                               : (ta.p1.z + ta.p2.z + ta.p3.z) / 3.0f;
        float cb = (axis == 0) ? (tb.p1.x + tb.p2.x + tb.p3.x) / 3.0f
                 : (axis == 1) ? (tb.p1.y + tb.p2.y + tb.p3.y) / 3.0f
                               : (tb.p1.z + tb.p2.z + tb.p3.z) / 3.0f;
        return ca < cb;
    };

    size_t mid = indices.size() / 2;
    std::nth_element(indices.begin(), indices.begin() + mid, indices.end(), comparator);

    std::vector<uint32_t> left(indices.begin(), indices.begin() + mid);
    std::vector<uint32_t> right(indices.begin() + mid, indices.end());

    node->left = BuildKDTree(tris, left, depth + 1);
    node->right = BuildKDTree(tris, right, depth + 1);
    return node;
}

void FreeKDTree(MapRaytrace::KDNode* node) {
    if (!node) return;
    FreeKDTree(node->left);
    FreeKDTree(node->right);
    delete node;
}

// ── Ray-AABB intersection (slab method) ──────────────────────────────

bool RayAABBIntersect(const MapRaytrace::Vec3& origin, const MapRaytrace::Vec3& inv_dir,
                      const MapRaytrace::AABB& box, float& tmin, float& tmax) {
    float t1 = (box.min.x - origin.x) * inv_dir.x;
    float t2 = (box.max.x - origin.x) * inv_dir.x;
    tmin = std::min(t1, t2);
    tmax = std::max(t1, t2);

    float t3 = (box.min.y - origin.y) * inv_dir.y;
    float t4 = (box.max.y - origin.y) * inv_dir.y;
    tmin = std::max(tmin, std::min(t3, t4));
    tmax = std::min(tmax, std::max(t3, t4));

    float t5 = (box.min.z - origin.z) * inv_dir.z;
    float t6 = (box.max.z - origin.z) * inv_dir.z;
    tmin = std::max(tmin, std::min(t5, t6));
    tmax = std::min(tmax, std::max(t5, t6));

    return tmax >= std::max(0.0f, tmin);
}

// ── Möller-Trumbore ray-triangle intersection ────────────────────────

bool RayTriangleIntersect(const MapRaytrace::Vec3& origin, const MapRaytrace::Vec3& dir,
                          const MapRaytrace::Triangle& tri, float& t) {
    MapRaytrace::Vec3 edge1, edge2, h, s, q;
    float a, f, u, v;

    edge1.x = tri.p2.x - tri.p1.x;
    edge1.y = tri.p2.y - tri.p1.y;
    edge1.z = tri.p2.z - tri.p1.z;

    edge2.x = tri.p3.x - tri.p1.x;
    edge2.y = tri.p3.y - tri.p1.y;
    edge2.z = tri.p3.z - tri.p1.z;

    // h = dir × edge2
    h.x = dir.y * edge2.z - dir.z * edge2.y;
    h.y = dir.z * edge2.x - dir.x * edge2.z;
    h.z = dir.x * edge2.y - dir.y * edge2.x;

    a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;

    if (a > -EPSILON && a < EPSILON)
        return false;

    f = 1.0f / a;

    s.x = origin.x - tri.p1.x;
    s.y = origin.y - tri.p1.y;
    s.z = origin.z - tri.p1.z;

    u = f * (s.x * h.x + s.y * h.y + s.z * h.z);
    if (u < 0.0f || u > 1.0f)
        return false;

    // q = s × edge1
    q.x = s.y * edge1.z - s.z * edge1.y;
    q.y = s.z * edge1.x - s.x * edge1.z;
    q.z = s.x * edge1.y - s.y * edge1.x;

    v = f * (dir.x * q.x + dir.y * q.y + dir.z * q.z);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z);
    return (t > EPSILON);
}

// ── KD-tree ray traversal ────────────────────────────────────────────

bool TraverseKDTree(MapRaytrace::KDNode* node,
                    const MapRaytrace::Vec3& origin,
                    const MapRaytrace::Vec3& dir,
                    const MapRaytrace::Vec3& inv_dir,
                    float max_dist) {
    if (!node) return false;

    float tmin, tmax;
    if (!RayAABBIntersect(origin, inv_dir, node->bbox, tmin, tmax))
        return false;

    // AABB is behind us or too far
    if (tmax < 0.0f || tmin > max_dist)
        return false;

    // Leaf node: test all triangles
    if (!node->triangle_indices.empty()) {
        for (uint32_t idx : node->triangle_indices) {
            float t;
            if (RayTriangleIntersect(origin, dir, g_triangles[idx], t)) {
                if (t < max_dist)
                    return true;  // blocked
            }
        }
        return false;
    }

    // Internal node: recurse both children
    // Visit the closer child first for early-out
    if (node->left && node->right) {
        // Determine which child is closer
        // (simple heuristic: just try both, the early-out handles the rest)
        if (TraverseKDTree(node->left, origin, dir, inv_dir, max_dist))
            return true;
        return TraverseKDTree(node->right, origin, dir, inv_dir, max_dist);
    }

    if (node->left)  return TraverseKDTree(node->left, origin, dir, inv_dir, max_dist);
    if (node->right) return TraverseKDTree(node->right, origin, dir, inv_dir, max_dist);
    return false;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════════════

void MapRaytrace::Init(const std::string& map_folder) {
    g_map_folder = map_folder;
    std::filesystem::create_directories(g_map_folder);
    LOGF(INFO, "[raytrace] init — map folder: {}", g_map_folder);
}

bool MapRaytrace::LoadMap(const std::string& map_name) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (map_name == g_current_map && g_ready)
        return true;

    // Free previous data
    if (g_root) {
        FreeKDTree(g_root);
        g_root = nullptr;
    }
    g_triangles.clear();
    g_ready = false;

    if (map_name.empty())
        return false;

    // Try multiple path patterns
    std::string path;
    for (const auto& candidate : {
        g_map_folder + "/" + map_name + ".tri",
        g_map_folder + "/" + map_name + "/world_physics.tri",
        map_name + ".tri",
    }) {
        if (std::filesystem::exists(candidate)) {
            path = candidate;
            break;
        }
    }

    if (path.empty()) {
        LOGF(WARNING, "[raytrace] .tri file not found for map '{}' — searched in {}",
             map_name, g_map_folder);
        return false;
    }

    // Read triangle data
    auto t0 = std::chrono::steady_clock::now();

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        LOGF(WARNING, "[raytrace] failed to open {}", path);
        return false;
    }

    auto file_size = in.tellg();
    if (file_size <= 0 || file_size % sizeof(Triangle) != 0) {
        LOGF(WARNING, "[raytrace] invalid .tri file size: {} bytes", (long long)file_size);
        return false;
    }

    size_t num_tris = file_size / sizeof(Triangle);
    g_triangles.resize(num_tris);
    in.seekg(0);
    in.read(reinterpret_cast<char*>(g_triangles.data()), file_size);
    in.close();

    // Build KD-tree
    std::vector<uint32_t> indices(num_tris);
    for (uint32_t i = 0; i < num_tris; ++i) indices[i] = i;

    g_root = BuildKDTree(g_triangles, indices, 0);

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    g_current_map = map_name;
    g_ready = true;

    LOGF(INFO, "[raytrace] loaded '{}' — {} triangles, KD-tree built in {:.1f}ms",
         map_name, num_tris, ms);

    return true;
}

void MapRaytrace::Unload() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_root) {
        FreeKDTree(g_root);
        g_root = nullptr;
    }
    g_triangles.clear();
    g_current_map.clear();
    g_ready = false;
    LOGF(INFO, "[raytrace] unloaded");
}

bool MapRaytrace::IsVisible(const Vec3& origin, const Vec3& target) {
    if (!g_ready || !g_root)
        return true;  // No map loaded — assume visible (don't block aim)

    Vec3 dir;
    dir.x = target.x - origin.x;
    dir.y = target.y - origin.y;
    dir.z = target.z - origin.z;

    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (length < 1.0f)
        return true;

    // Normalize direction
    dir.x /= length;
    dir.y /= length;
    dir.z /= length;

    // Precompute inverse direction for slab test (avoid div-by-zero)
    Vec3 inv_dir;
    inv_dir.x = (std::fabs(dir.x) < EPSILON) ? 1e30f : 1.0f / dir.x;
    inv_dir.y = (std::fabs(dir.y) < EPSILON) ? 1e30f : 1.0f / dir.y;
    inv_dir.z = (std::fabs(dir.z) < EPSILON) ? 1e30f : 1.0f / dir.z;

    std::lock_guard<std::mutex> lock(g_mutex);
    return !TraverseKDTree(g_root, origin, dir, inv_dir, length);
}

const std::string& MapRaytrace::CurrentMap() {
    return g_current_map;
}

bool MapRaytrace::IsReady() {
    return g_ready;
}
