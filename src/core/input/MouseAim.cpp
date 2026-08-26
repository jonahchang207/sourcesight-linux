#include "MouseAim.hpp"

#include "common.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/classes/Player.hpp"
#include "core/input/AimHotkey.hpp"
#include "gui/renderer/Renderer.hpp" // Menu-open guard (same as Macro.cpp)

#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

namespace {

constexpr double kResyncInterval = 0.25;  // seconds between hyprctl cursor re-queries
constexpr double kMaxDt = 0.1;            // clamp a single frame time (s)
constexpr float kTargetLostRadius = 48.0f; // px: how far a target may drift before lock breaks

double NowSeconds() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

// Query the focused (or first) monitor geometry through Hyprland, falling
// back to 1080p when the compositor cannot be reached.
std::pair<float, float> MonitorSize() {
    const std::pair<float, float> fallback{ 1920.0f, 1080.0f };
    FILE* pipe = ::popen("hyprctl monitors -j", "r");
    if (!pipe)
        return fallback;

    std::string text;
    char buffer[512];
    size_t n;
    while ((n = ::fread(buffer, 1, sizeof(buffer), pipe)) > 0)
        text.append(buffer, n);
    const int result = ::pclose(pipe);
    if (result != 0 || text.empty())
        return fallback;

    try {
        const auto monitors = nlohmann::json::parse(text);
        if (!monitors.is_array())
            return fallback;
        std::pair<float, float> first{ 0.0f, 0.0f };
        for (const auto& monitor : monitors) {
            if (!monitor.is_object() || !monitor.contains("width") || !monitor.contains("height"))
                continue;
            const float width = monitor["width"].get<float>();
            const float height = monitor["height"].get<float>();
            if (width <= 0.0f || height <= 0.0f)
                continue;
            if (monitor.value("focused", false))
                return { width, height };
            if (first.first <= 0.0f)
                first = { width, height };
        }
        if (first.first > 0.0f)
            return first;
    }
    catch (const std::exception&) {
    }
    return fallback;
}

// World-space head position read straight from memory (head bone), with the
// eye height as a fallback when the skeleton is unavailable.
Vec3_t PlayerHeadWorld(const Player& player) {
    if (player.bone_list.size() > static_cast<size_t>(bone_index::head))
        return player.bone_list[bone_index::head].pos;
    return player.pos + Vec3_t(0.f, 0.f, 64.f);
}

// World-space aim position based on cfg::aim::target_part:
//   0 = head, 1 = body (chest), 2 = legs (mid-knee),
//   3 = in-between body & head (neck/spine1 blend)
Vec3_t PlayerAimWorld(const Player& player) {
    const auto& bones = player.bone_list;
    const int part = cfg::aim::target_part;

    if (part == 0) {
        // Head
        if (bones.size() > static_cast<size_t>(bone_index::head))
            return bones[bone_index::head].pos;
        return player.pos + Vec3_t(0.f, 0.f, 64.f);
    }
    if (part == 1) {
        // Body (chest)
        if (bones.size() > static_cast<size_t>(bone_index::chest))
            return bones[bone_index::chest].pos;
        return player.pos + Vec3_t(0.f, 0.f, 48.f);
    }
    if (part == 2) {
        // Legs (midpoint between knees)
        if (bones.size() > static_cast<size_t>(bone_index::knee_L) &&
            bones.size() > static_cast<size_t>(bone_index::knee_R))
            return (bones[bone_index::knee_L].pos + bones[bone_index::knee_R].pos) * 0.5f;
        return player.pos + Vec3_t(0.f, 0.f, 20.f);
    }
    // part == 3: in-between body & head — blend neck and spine1
    if (bones.size() > static_cast<size_t>(bone_index::neck) &&
        bones.size() > static_cast<size_t>(bone_index::spine_1))
        return (bones[bone_index::neck].pos * 0.6f + bones[bone_index::spine_1].pos * 0.4f);
    return player.pos + Vec3_t(0.f, 0.f, 56.f);
}

// Head position in screen coordinates: aim bone -> view matrix -> overlay
// pixels. Falls back to the ESP box top-centre when the bone does not
// project (e.g. the player is only partially on screen).
bool PlayerAimScreen(Player& player, view_matrix_t& matrix,
                     float width, float height, Vec2_t& out) {
    if (matrix.wts(PlayerAimWorld(player), Vec2_t(width, height), out))
        return true;
    std::pair<Vec2_t, Vec2_t> bounds;
    if (player.GetBounds(matrix, Vec2_t(width, height), bounds)) {
        out.x = (bounds.first.x + bounds.second.x) * 0.5f;
        out.y = bounds.first.y + (bounds.second.y - bounds.first.y) * 0.15f;
        return true;
    }
    return false;
}

// Ray vs. axis-aligned box (slab method). Returns the entry distance t along
// the ray, or -1 when there is no hit.
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

// World-space eye position of a player (head bone, else eye height).
Vec3_t PlayerEye(const Player& player) {
    if (player.bone_list.size() > static_cast<size_t>(bone_index::head))
        return player.bone_list[bone_index::head].pos;
    return player.pos + Vec3_t(0.f, 0.f, 64.f);
}

// Vision check: is the segment from ``eye`` to ``head`` blocked by any other
// alive player's body? (World walls are not traceable from an external
// process; this is the practical line-of-sight proxy.)
bool HeadOccluded(const Vec3_t& eye, const Vec3_t& head,
                  const std::vector<Player>& players, int skip_index) {
    const Vec3_t delta = head - eye;
    const float dist = delta.length();
    if (dist < 1.0f)
        return false;
    const Vec3_t dir = delta / dist;
    for (const auto& target : players) {
        if (!target.alive || target.localplayer || target.index == skip_index)
            continue;
        const Vec3_t center = target.pos + Vec3_t(0.f, 0.f, 36.f);
        const float t = RayBox(eye, dir, center, Vec3_t(16.f, 16.f, 36.f));
        if (t >= 0.0f && t < dist)
            return true;
    }
    return false;
}

} // namespace

// MB5 side-button toggle; owned here for the same lifecycle reasons.
static AimHotkey g_hotkey;

std::mutex MouseAim::mtx_;
bool MouseAim::inited_ = false;
float MouseAim::screen_w_ = 1920.0f;
float MouseAim::screen_h_ = 1080.0f;

bool MouseAim::cursor_tracked_ = false;
float MouseAim::cursor_x_ = 0.0f;
float MouseAim::cursor_y_ = 0.0f;
double MouseAim::last_resync_ = 0.0;

bool MouseAim::has_target_ = false;
float MouseAim::target_x_ = 0.0f;
float MouseAim::target_y_ = 0.0f;
bool MouseAim::auto_target_ = false;
int MouseAim::locked_player_index_ = -1;
bool MouseAim::target_visible_ = false;

double MouseAim::last_update_ = 0.0;
int MouseAim::prev_locked_index_ = -1;

KernelMouse& MouseAim::Mouse() {
    static KernelMouse mouse;
    return mouse;
}

bool MouseAim::Available() {
    std::lock_guard<std::mutex> lock(mtx_);
    return Mouse().Available();
}

bool MouseAim::DriverInstalled() {
    struct stat st {};
    return ::stat("/dev/person-mouse", &st) == 0 && S_ISCHR(st.st_mode);
}

bool MouseAim::TrackedCursor(float& x, float& y) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!cursor_tracked_ || cfg::aim::game_mode)
        return false;
    x = cursor_x_;
    y = cursor_y_;
    return true;
}

void MouseAim::ScreenSize(float& width, float& height) {
    std::lock_guard<std::mutex> lock(mtx_);
    width = screen_w_;
    height = screen_h_;
}

void MouseAim::ReferencePoint(float& x, float& y) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!cfg::aim::game_mode && cursor_tracked_) {
        x = cursor_x_;
        y = cursor_y_;
    }
    else {
        x = screen_w_ * 0.5f;
        y = screen_h_ * 0.5f;
    }
}

void MouseAim::SetTarget(float x, float y) {
    std::lock_guard<std::mutex> lock(mtx_);
    SetTargetUnlocked(x, y);
}

void MouseAim::ClearTarget() {
    std::lock_guard<std::mutex> lock(mtx_);
    ClearTargetUnlocked();
}

void MouseAim::SetTargetUnlocked(float x, float y) {
    has_target_ = true;
    target_x_ = x;
    target_y_ = y;
    // Explicit targets (colour scanner) are trusted until replaced.
    auto_target_ = false;
}

void MouseAim::ClearTargetUnlocked() {
    has_target_ = false;
    auto_target_ = false;
    locked_player_index_ = -1;
    target_visible_ = false;
}

bool MouseAim::TargetInfo(float& x, float& y, bool& visible) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!has_target_)
        return false;
    x = target_x_;
    y = target_y_;
    visible = target_visible_;
    return true;
}

void MouseAim::Init() {
    const auto size = MonitorSize();
    screen_w_ = size.first;
    screen_h_ = size.second;
    Mouse().Open(static_cast<int>(screen_w_), static_cast<int>(screen_h_));
    if (Mouse().Available()) {
        LOGF(INFO, "[aim] kernel mouse ready on /dev/person-mouse ({}x{})",
             static_cast<int>(screen_w_), static_cast<int>(screen_h_));
    }
    else {
        LOGF(WARNING, "[aim] /dev/person-mouse unavailable - install it with sudo ./drivers/install.sh");
    }
    last_update_ = NowSeconds();
    last_resync_ = 0.0;
}

void MouseAim::MaybeResyncCursor(double now) {
    if (cfg::aim::game_mode)
        return;
    if (now - last_resync_ < kResyncInterval)
        return;
    last_resync_ = now;

    const auto position = KernelMouse::CursorPosition();
    if (!position)
        return;

    cursor_x_ = static_cast<float>(position->first);
    cursor_y_ = static_cast<float>(position->second);
    cursor_tracked_ = true;
}

int MouseAim::ValidateTarget() {
    // The locked target is stale once no alive enemy is projected near it;
    // without this the controller would keep aiming at a dead player.
    // Returns the player index of the match, or -1 when the lock is lost.
    // (Same-thread access: the engine thread owns both Refresh() and Update().)
    auto& cache = Cache::Get();
    auto& matrix = cache.game.view_matrix;
    const auto& local = cache.local;

    for (auto& player : cache.players) {
        if (!player.alive || player.localplayer)
            continue;
        if (player.team == local.team)
            continue;
        Vec2_t screen;
        if (!PlayerAimScreen(player, matrix, screen_w_, screen_h_, screen))
            continue;
        const float dx = screen.x - target_x_;
        const float dy = screen.y - target_y_;
        if (dx * dx + dy * dy <= kTargetLostRadius * kTargetLostRadius)
            return player.index;
    }
    return -1;
}

bool MouseAim::SelectNearestEnemy() {
    auto& cache = Cache::Get();
    auto& matrix = cache.game.view_matrix;
    const auto& local = cache.local;
    const Vec3_t eye = PlayerEye(local);

    float ref_x = screen_w_ * 0.5f;
    float ref_y = screen_h_ * 0.5f;
    if (!cfg::aim::game_mode && cursor_tracked_) {
        ref_x = cursor_x_;
        ref_y = cursor_y_;
    }

    const float fov = std::max(1.0f, cfg::aim::fov_radius);
    const float fov_sq = fov * fov;

    int best_index = -1;
    float best_x = 0.0f;
    float best_y = 0.0f;
    // When visible_only is on, only pass 0 runs (visible targets only).
    // When off, pass 0 = visible, pass 1 = hidden fallback.
    const int max_passes = cfg::aim::visible_only ? 1 : 2;
    for (int pass = 0; pass < max_passes && best_index < 0; ++pass) {
        float best_dist = std::numeric_limits<float>::max();
        for (auto& player : cache.players) {
            if (!player.alive || player.localplayer)
                continue;
            if (player.team == local.team)
                continue;

            Vec2_t screen;
            if (!PlayerAimScreen(player, matrix, screen_w_, screen_h_, screen))
                continue;

            const float dx = screen.x - ref_x;
            const float dy = screen.y - ref_y;
            const float dist_sq = dx * dx + dy * dy;
            if (dist_sq > fov_sq)
                continue;
            if (pass == 0 && HeadOccluded(eye, PlayerHeadWorld(player), cache.players, player.index))
                continue;
            if (dist_sq < best_dist) {
                best_dist = dist_sq;
                best_x = screen.x;
                best_y = screen.y;
                best_index = player.index;
            }
        }
    }

    if (best_index < 0) {
        ClearTargetUnlocked();
        return false;
    }
    SetTargetUnlocked(best_x, best_y);
    auto_target_ = true;
    locked_player_index_ = best_index;
    return true;
}

void MouseAim::Update() {
    g_hotkey.Sync(cfg::aim::hotkey && cfg::aim::enabled);

    // MB5 (side button) toggles aim on/off without opening the menu.
    if (g_hotkey.ConsumeToggle()) {
        cfg::aim::enabled = !cfg::aim::enabled;
        LOGF(INFO, "[aim] toggled {}", cfg::aim::enabled ? "ON" : "OFF");
    }

    // Panic key (F9): instantly disable all cheats.
    if (cfg::settings::panic_key && ImGui::IsKeyPressed(ImGuiKey_F9)) {
        cfg::aim::enabled = false;
        cfg::esp::spotted_only = false;
        cfg::esp::headshot_line = false;
        LOGF(WARNING, "[panic] all cheats disabled (F9)");
    }

    if (!cfg::aim::enabled) {
        if (inited_)
            ClearTarget();
        return;
    }

    // While the menu is open the user is configuring, not playing: never
    // fight the user's own mouse (same guard as the macro module).
    if (Renderer::IsOpen()) {
        if (inited_)
            ClearTarget();
        return;
    }

    std::lock_guard<std::mutex> lock(mtx_);

    if (!inited_) {
        inited_ = true;
        Init();
    }
    if (!Mouse().Available())
        return;

    const double now = NowSeconds();
    const float dt = static_cast<float>(std::clamp(now - last_update_, 0.0, kMaxDt));
    last_update_ = now;

    // ── target selection ────────────────────────────────────────────
    // The magenta scanner feeds targets of its own; the memory-based enemy
    // picker only runs when the scanner is off.
    if (!has_target_ && cfg::aim::aim_at_enemies)
        SelectNearestEnemy();
    if (!has_target_) {
        MaybeResyncCursor(now);
        return;
    }

    // ── reference point ─────────────────────────────────────────────
    float ref_x = screen_w_ * 0.5f;
    float ref_y = screen_h_ * 0.5f;
    if (!cfg::aim::game_mode) {
        MaybeResyncCursor(now);
        if (!cursor_tracked_)
            return;
        ref_x = cursor_x_;
        ref_y = cursor_y_;
    }

    const float error_x = target_x_ - ref_x;
    const float error_y = target_y_ - ref_y;

    // ── FOV gate: only pursue targets inside the ring around the cursor ──
    const float fov = std::max(1.0f, cfg::aim::fov_radius);
    if (error_x * error_x + error_y * error_y > fov * fov) {
        ClearTargetUnlocked();
        return;
    }

    // A stale auto-locked target (enemy died / left the screen) must release
    // the lock so SelectNearestEnemy can re-evaluate next tick. Only targets
    // picked by the auto selection are validated; scanner targets are trusted.
    if (cfg::aim::aim_at_enemies && auto_target_ && has_target_) {
        const int locked_index = ValidateTarget();
        if (locked_index < 0) {
            ClearTargetUnlocked();
            return;
        }
        locked_player_index_ = locked_index;
    }

    auto& cache = Cache::Get();

    // ── vision check: is the locked head in our line of sight? ───────
    if (auto_target_ && locked_player_index_ >= 0) {
        bool found = false;
        Vec3_t head;
        for (auto& p : cache.players) {
            if (p.index == locked_player_index_) {
                head = PlayerHeadWorld(p);
                found = true;
                break;
            }
        }
        target_visible_ = !found ||
            !HeadOccluded(PlayerEye(cache.local), head, cache.players, locked_player_index_);
    }
    else {
        target_visible_ = true;
    }

    // ── target switch delay ────────────────────────────────────────
    if (cfg::aim::target_switch_delay > 0.0f && auto_target_) {
        static double last_switch_time = 0.0;
        if (locked_player_index_ != prev_locked_index_) {
            if (now - last_switch_time < cfg::aim::target_switch_delay) {
                locked_player_index_ = prev_locked_index_;
                return;
            }
            last_switch_time = now;
        }
        prev_locked_index_ = locked_player_index_;
    }

    // ── weapon-specific speed multiplier ────────────────────────────
    float weapon_mult = cfg::aim::rifle_mult;
    if (auto_target_ && locked_player_index_ >= 0) {
        for (auto& p : cache.players) {
            if (p.index == locked_player_index_) {
                const auto& wname = p.weapon.name;
                if (wname.find("Pistol") != std::string::npos ||
                    wname.find("Glock") != std::string::npos ||
                    wname.find("USP") != std::string::npos ||
                    wname.find("P250") != std::string::npos ||
                    wname.find("Deagle") != std::string::npos ||
                    wname.find("Five-SeveN") != std::string::npos ||
                    wname.find("Tec-9") != std::string::npos ||
                    wname.find("Dual Berettas") != std::string::npos)
                    weapon_mult = cfg::aim::pistol_mult;
                else if (wname.find("AWP") != std::string::npos ||
                         wname.find("Scout") != std::string::npos ||
                         wname.find("G3SG1") != std::string::npos ||
                         wname.find("SCAR-20") != std::string::npos)
                    weapon_mult = cfg::aim::sniper_mult;
                else if (wname.find("MP9") != std::string::npos ||
                         wname.find("MAC-10") != std::string::npos ||
                         wname.find("MP7") != std::string::npos ||
                         wname.find("MP5-SD") != std::string::npos ||
                         wname.find("UMP-45") != std::string::npos ||
                         wname.find("P90") != std::string::npos ||
                         wname.find("PP-Bizon") != std::string::npos)
                    weapon_mult = cfg::aim::smg_mult;
                break;
            }
        }
    }

    // ── movement algorithm → desired position ───────────────────────
    float dx = target_x_ - ref_x;
    float dy = target_y_ - ref_y;

    // ── speed caps: pixels/second ceiling plus a hard per-tick ceiling ──
    const float base_speed = std::max(0.0f, cfg::aim::speed) * weapon_mult;
    const float max_delta = std::max(1.0f, cfg::aim::max_delta);
    const float speed_cap = std::max(1.0f, base_speed * dt);
    const float cap = std::min(max_delta, speed_cap);
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length > cap) {
        dx *= cap / length;
        dy *= cap / length;
    }

    // ── deadzone ───────────────────────────────────────────────────
    const float deadzone = std::max(0.0f, cfg::aim::deadzone);
    if (std::fabs(dx) <= deadzone)
        dx = 0.0f;
    if (std::fabs(dy) <= deadzone)
        dy = 0.0f;
    if (dx == 0.0f && dy == 0.0f)
        return;

    // ── bypass: add micro-noise to look human ──────────────────────
    if (cfg::bypass::humanize_movement) {
        const float noise = cfg::bypass::noise_amplitude;
        dx += (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f * noise;
        dy += (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f * noise;
    }

    if (Mouse().Move(static_cast<int>(std::lround(dx)), static_cast<int>(std::lround(dy)))) {
        if (!cfg::aim::game_mode) {
            cursor_x_ = std::clamp(cursor_x_ + dx, 0.0f, screen_w_);
            cursor_y_ = std::clamp(cursor_y_ + dy, 0.0f, screen_h_);
        }
    }
}
