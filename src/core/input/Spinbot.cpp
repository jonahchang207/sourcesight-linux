#include "core/input/Spinbot.hpp"

#include <chrono>
#include <cmath>

#include "config/Current.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/Engine.hpp"
#include "core/input/KernelMouse.hpp"
#include "core/input/MouseAim.hpp"
#include "core/input/Triggerbot.hpp"
#include "gui/renderer/Renderer.hpp"

namespace {

double SpinNow() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

// Kernel-virtual mouse owned by the spinbot. Opened lazily with the same
// virtual screen bounds the aim controller uses so the device is consistent.
KernelMouse& SpinMouse() {
    static KernelMouse mouse;
    static bool opened = false;
    if (!opened) {
        float w = 1920.0f, h = 1080.0f;
        MouseAim::ScreenSize(w, h);
        opened = mouse.Open(static_cast<int>(w), static_cast<int>(h));
    }
    return mouse;
}

} // namespace

static bool g_inited = false;
static double g_last = 0.0;
static double g_t = 0.0; // running spin phase, drives the sway waveform

void Spinbot::Init() {
    g_inited = true;
    g_last = SpinNow();
}

void Spinbot::Update() {
    UpdateImpl();
}

void Spinbot::UpdateImpl() {
    if (!g_inited) {
        g_inited = true;
        g_last = SpinNow();
    }
    if (!cfg::spinbot::enabled) {
        g_last = SpinNow(); // reset phase so a re-enable starts clean
        return;
    }
    // Never fight the user's own mouse while the menu is open.
    if (Renderer::IsOpen())
        return;
    if (!SpinMouse().Available())
        return;

    const double now = SpinNow();
    const double dt = std::clamp(now - g_last, 0.0, 0.1);
    g_last = now;
    g_t += dt;

    // Yaw: constant horizontal mouse movement. Delta-time scaled so the spin
    // rate is tick-rate independent.
    const float speed = std::max(0.0f, cfg::spinbot::speed);
    const int dir = cfg::spinbot::direction >= 0 ? 1 : -1;
    const float kTwoPi = 6.28318530718f;
    float dx = speed * static_cast<float>(dt) * static_cast<float>(dir);

    // Pitch: gentle sway adds depth so a fast spin isn't a dead flat circle.
    float dy = 0.0f;
    if (cfg::spinbot::pitch_sway > 0.0f) {
        const float amp = std::max(0.0f, cfg::spinbot::pitch_sway);
        const float freq = std::max(0.2f, cfg::spinbot::sway_hz);
        // dy = d/dt [ amp * sin(2pi f t) ] — derivative keeps it snappy at
        // any tick rate.
        dy = amp * kTwoPi * freq *
             std::cos(kTwoPi * freq * static_cast<float>(g_t)) *
             static_cast<float>(dt);
    }

    const float kMin = 0.5f;
    if (std::fabs(dx) < kMin && std::fabs(dy) < kMin)
        return;

    SpinMouse().Move(static_cast<int>(std::lround(dx)), static_cast<int>(std::lround(dy)));

    // ── perfect shooting ─────────────────────────────────────────────
    // Fire exactly one shot the instant the crosshair sweeps onto an
    // enemy — a clean shot per pass, no sprayed rounds. Reuses the
    // triggerbot's projection check (visible_only / target_part /
    // threshold), so behavior matches your trigger settings. Needs the
    // master switch and a live local player.
    if (!cfg::spinbot::shoot || !cfg::enabled)
        return;
    auto& cache = Cache::Get();
    if (!cache.local.alive)
        return;

    static bool prev_on_enemy = false;
    static double last_fire = 0.0;
    const bool on_enemy = Triggerbot::OnEnemy();
    if (on_enemy && !prev_on_enemy && (now - last_fire) > 0.12)
        last_fire = Triggerbot::Fire() ? now : last_fire;
    prev_on_enemy = on_enemy;
}