#include "Triggerbot.hpp"

#include "common.hpp"
#include "config/Current.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/Engine.hpp"
#include "core/input/MouseAim.hpp"
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

// How close (px) the aim point must be to the crosshair for the locked-on
// burst to arm, and how long it re-arms after a burst cycle.
constexpr float kLockRadius = 10.0f;

void InitDisplay() {
    if (!g_display)
        g_display = XOpenDisplay(nullptr);
}

// Send key releases for the strafe keys (A/D) via XTest so the shot is
// taken while standing still. Running this every tick is harmless; XTest
// just reports the (already up) key as up.
void ReleaseStrafe() {
    if (!g_display) return;
    static const KeySym strafe_syms[] = { XK_a, XK_d };
    for (KeySym sym : strafe_syms) {
        const KeyCode code = XKeysymToKeycode(g_display, sym);
        if (code)
            XTestFakeKeyEvent(g_display, code, False, CurrentTime);
    }
    XFlush(g_display);
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

    // Linked aim+trigger owns the firing while it is enabled, so the manual
    // crosshair check is skipped to avoid double shots.
    if (cfg::aim::enabled && cfg::aim::lock_burst)
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

    // On-target check. When the aim is actively locked on the enemy we
    // trust its (EMA-smoothed) lock distance against the configured
    // threshold — steady even under aim jitter. Otherwise fall back to the
    // raw head projection, same radius.
    const bool on_enemy = IsCrosshairOnEnemy();
    const bool aim_locked_on = cfg::aim::enabled && MouseAim::Locked() &&
                               MouseAim::OnTarget(cfg::triggerbot::threshold);

    // Dwell debounce: the aim point and raw projection jitter a pixel or
    // two per frame; hold on-target briefly so a transient flick can't fire
    // a burst on empty air. Small enough to stay snappy when used alongside
    // the aim, large enough to silence its noise.
    static double on_target_since = 0.0;
    using namespace std::chrono;
    static const auto dwell_epoch = steady_clock::now();
    const double now_s = duration<double>(steady_clock::now() - dwell_epoch).count();
    if (!on_enemy && !aim_locked_on) {
        on_target_since = 0.0;
        return;
    }
    if (on_target_since == 0.0)
        on_target_since = now_s;
    if ((now_s - on_target_since) * 1000.0 < cfg::triggerbot::dwell_ms)
        return;

    Burst();
}

bool Triggerbot::IsCrosshairOnEnemy() {
    auto& cache = Cache::Get();
    const auto& local = cache.local;
    const auto& players = cache.players;
    const auto& game = cache.game;

    const float crosshair_radius = std::max(1.0f, cfg::triggerbot::threshold); // px: how close to crosshair

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

void Triggerbot::Burst() {
    InitDisplay();
    if (!g_display)
        return;

    if (cfg::triggerbot::delay_ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg::triggerbot::delay_ms));

    for (int i = 0; i < cfg::triggerbot::burst_count; ++i) {
        if (i > 0 && cfg::triggerbot::burst_delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg::triggerbot::burst_delay_ms));
        XTestClick(g_display);
    }
}

void Triggerbot::UpdateAimLink() {
    if (!cfg::enabled || !cfg::aim::enabled || !cfg::aim::lock_burst)
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

    // Only act once the aim has actually landed on the locked target.
    if (!MouseAim::Locked() || !MouseAim::OnTarget(kLockRadius))
        return;

    // Stop A/D strafing so the burst is taken from a standstill.
    ReleaseStrafe();

    // Sustain bursts while tracking: re-arm once one full cycle has elapsed.
    static double last_burst = 0.0;
    using namespace std::chrono;
    static const auto epoch = steady_clock::now();
    const double now = duration<double>(steady_clock::now() - epoch).count();
    double cooldown =
        cfg::triggerbot::delay_ms +
        cfg::triggerbot::burst_count * cfg::triggerbot::burst_delay_ms + 180.0;
    cooldown = std::clamp(cooldown, 100.0, 800.0);
    if (now - last_burst < cooldown)
        return;
    last_burst = now;

    Burst();
}
