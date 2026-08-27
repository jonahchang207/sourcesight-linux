#include "Triggerbot.hpp"

#include "common.hpp"
#include "config/Current.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/Engine.hpp"
#include "gui/renderer/Renderer.hpp"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace {

Display* g_display = nullptr;

void InitDisplay() {
    if (!g_display)
        g_display = XOpenDisplay(nullptr);
}

// XTest left-click: press and release with a small delay.
void XTestClick(Display* d) {
    if (!d) return;
    XTestFakeButtonEvent(d, 1, True, CurrentTime);
    XFlush(d);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    XTestFakeButtonEvent(d, 1, False, CurrentTime);
    XFlush(d);
}

// Check if a world-space point projects to near the screen center.
// Returns true if the projected position is within `radius` pixels of center.
bool IsNearCrosshair(const Vec3_t& world_pos, view_matrix_t matrix,
                      float screen_w, float screen_h, float radius) {
    Vec2_t screen;
    if (!matrix.wts(world_pos, ImVec2(screen_w, screen_h), screen))
        return false;

    float cx = screen_w * 0.5f;
    float cy = screen_h * 0.5f;
    float dx = screen.x - cx;
    float dy = screen.y - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

// Get the effective head/neck position for crosshair check.
Vec3_t GetTargetPos(const Player& player, int target_part) {
    const auto& bones = player.bone_list;

    if (target_part == 0) {
        // Head
        if (bones.size() > static_cast<size_t>(bone_index::head))
            return bones[bone_index::head].pos;
        return player.pos + Vec3_t(0.f, 0.f, 64.f);
    }
    if (target_part == 1) {
        // Body (chest)
        if (bones.size() > static_cast<size_t>(bone_index::chest))
            return bones[bone_index::chest].pos;
        return player.pos + Vec3_t(0.f, 0.f, 48.f);
    }
    if (target_part == 2) {
        // Legs
        if (bones.size() > static_cast<size_t>(bone_index::knee_L) &&
            bones.size() > static_cast<size_t>(bone_index::knee_R))
            return (bones[bone_index::knee_L].pos + bones[bone_index::knee_R].pos) * 0.5f;
        return player.pos + Vec3_t(0.f, 0.f, 20.f);
    }
    // Default: neck / mid-body blend
    if (bones.size() > static_cast<size_t>(bone_index::head) &&
        bones.size() > static_cast<size_t>(bone_index::chest))
        return (bones[bone_index::head].pos + bones[bone_index::chest].pos) * 0.5f;
    return player.pos + Vec3_t(0.f, 0.f, 56.f);
}

} // anonymous namespace

void Triggerbot::Init() {
    InitDisplay();
}

void Triggerbot::Update() {
    UpdateImpl();
}

void Triggerbot::UpdateImpl() {
    if (!cfg::enabled || !cfg::triggerbot::enabled)
        return;

    if (Renderer::IsOpen())
        return;

    InitDisplay();
    if (!g_display)
        return;

    auto& cache = Cache::Get();
    const auto& local = cache.local;
    if (!local.alive)
        return;

    // Hotkey gate: when hotkey mode is on, only fire while the key is held.
    // We check the X11 key state for a configurable key (we'll use Left Alt
    // as the trigger key since it's commonly unused in CS2).
    if (cfg::triggerbot::hotkey) {
        char keys[32] = {};
        XQueryKeymap(g_display, keys);
        KeyCode alt_code = XKeysymToKeycode(g_display, XK_Alt_L);
        bool held = alt_code && (keys[alt_code / 8] & (1 << (alt_code % 8)));
        if (!held)
            return;
    }

    // Weapon filter
    const short weapon_id = local.weapon.item_index;
    if (cfg::triggerbot::pistols_only) {
        // Check if weapon is a pistol (IDs roughly 1-7, 30-32, 64)
        bool is_pistol = (weapon_id >= 1 && weapon_id <= 7) ||
                         (weapon_id >= 30 && weapon_id <= 32) ||
                         weapon_id == 64;
        if (!is_pistol) return;
    }
    if (cfg::triggerbot::rifles_only) {
        // Check if weapon is a rifle (AK, M4, AUG, SG, FAMAS, Galil, AWP, Scout, etc.)
        bool is_rifle = (weapon_id == 7 || weapon_id == 8 || weapon_id == 9 ||
                         weapon_id == 10 || weapon_id == 11 || weapon_id == 13 ||
                         weapon_id == 16 || weapon_id == 38 || weapon_id == 39 ||
                         weapon_id == 40);
        if (!is_rifle) return;
    }

    if (!IsCrosshairOnEnemy())
        return;

    // Fire with optional delay and burst
    if (cfg::triggerbot::delay_ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg::triggerbot::delay_ms));

    for (int i = 0; i < cfg::triggerbot::burst_count; ++i) {
        if (i > 0 && cfg::triggerbot::burst_delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg::triggerbot::burst_delay_ms));
        XTestClick(g_display);
    }
}

bool Triggerbot::IsCrosshairOnEnemy() {
    auto& cache = Cache::Get();
    const auto& local = cache.local;
    const auto& players = cache.players;
    const auto& game = cache.game;

    const float crosshair_radius = 12.0f; // pixels: how close to crosshair

    for (const auto& player : players) {
        if (!player.alive)
            continue;
        if (player.localplayer)
            continue;
        if (player.team == local.team)
            continue;

        // Spotted check
        if (cfg::triggerbot::visible_only && !player.spotted)
            continue;

        // Get target position based on target_part setting
        Vec3_t target_pos = GetTargetPos(player, cfg::aim::target_part);

        if (IsNearCrosshair(target_pos, game.view_matrix,
                            ImGui::GetIO().DisplaySize.x,
                            ImGui::GetIO().DisplaySize.y,
                            crosshair_radius)) {
            return true;
        }
    }

    return false;
}

void Triggerbot::Fire() {
    InitDisplay();
    if (g_display)
        XTestClick(g_display);
}
