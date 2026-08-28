#include "Renderer.hpp"
#include "window/Window.hpp"

#include "config/Current.hpp"
#include "core/engine/Engine.hpp"
#include "gui/frontend/esp/Esp.hpp"
#include "gui/frontend/menu/Menu.hpp"
#include "gui/frontend/overlays/Overlays.hpp"
#ifndef _WIN32
#include <X11/keysym.h>
#include <filesystem>
#include <unistd.h>
#include <algorithm>
#endif

#ifndef _WIN32
// Union of the menu rect and every movable / resizable overlay widget rect,
// clamped to the display. Returns x, y, w, h (w/h zero when nothing should
// be captured); each rect is inflated by `pad` so mid-drag drift keeps the
// pointer inside the XShape input region.
static void ComputeCaptureUnion(float out[4]) {
    auto& io = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    float L = W, T = H, R = 0.f, B = 0.f;
    const auto include = [&](float x, float y, float w, float h, float pad) {
        if (w <= 0.f || h <= 0.f)
            return;
        L = std::min(L, x - pad);
        T = std::min(T, y - pad);
        R = std::max(R, x + w + pad);
        B = std::max(B, y + h + pad);
    };

    const auto menu_pos = Menu::GetPos();
    const auto menu_size = Menu::GetSize();
    include(menu_pos.x, menu_pos.y, menu_size.x, menu_size.y, 4.f);

    // Movable / resizable widgets; sized generously since the actual window
    // decorations also live inside these rects.
    include(cfg::world::radar::pos.x, cfg::world::radar::pos.y,
            cfg::world::radar::size.x, cfg::world::radar::size.y, 40.f);
    include(cfg::world::velocity::pos.x, cfg::world::velocity::pos.y,
            cfg::world::velocity::size.x, cfg::world::velocity::size.y, 40.f);
    include(cfg::world::bomb::pos.x, cfg::world::bomb::pos.y, 200.f, 56.f, 40.f);
    include(cfg::world::spectators::pos.x, cfg::world::spectators::pos.y, 260.f, 160.f, 40.f);

    L = std::clamp(L, 0.f, W);
    R = std::clamp(R, 0.f, W);
    T = std::clamp(T, 0.f, H);
    B = std::clamp(B, 0.f, H);

    out[0] = L;
    out[1] = T;
    out[2] = (R > L) ? R - L : 0.f;
    out[3] = (B > T) ? B - T : 0.f;
}
#endif

bool Renderer::Init() {
    return GetInstance().InitImpl();
}

void Renderer::Thread() {
    return GetInstance().ThreadImpl();
}

void Renderer::Destroy() {
    return GetInstance().DestroyImpl();
}   

bool Renderer::IsOpen() {
    return GetInstance().isOpen;
}

bool Renderer::IsFocused() {
    return GetInstance().isFocused;
}

bool Renderer::InitImpl() {
    if (!Window::SpawnWindow()) {
        LOGF(WARNING, "Failed to create window; check DISPLAY/WAYLAND_DISPLAY and session permissions");
        return false;
    }

    if (!Window::CreateDevice()) {
        LOGF(WARNING, "Failed to create device");
        Window::DespawnWindow();
        return false;
    }

    if (!Window::CreateImGui()) {
        LOGF(FATAL, "Failed to create ImGui");
        Window::DestroyDevice();
        Window::DespawnWindow();
        return false;
    }

    if (!Menu::Init() || !Esp::Init() || !Overlays::Init()) {
        LOGF(FATAL, "Failed to initialize the interface");
        Window::DestroyImGui();
        Window::DestroyDevice();
        Window::DespawnWindow();
        return false;
    }

#ifdef _WIN32
    // Focus the game
    SetForegroundWindow(Engine::GetProcess()->hwnd_);
#endif

    if (cfg::settings::streamproof)
        Window::SetAffinity(Window::hwnd, WindowAffinity::Invisible);

    if (cfg::settings::vsync)
        Window::vsync = true;

    // We want the main thread to call render
    // And lock it
    // std::thread(Thread).detach();

    LOGF(INFO, "Successfully initialized renderer...");
    return true;
}

void Renderer::DestroyImpl() {
    isRunning = false; // Prepare to stop thread loop
    LOGF(VERBOSE, "Renderer shutdown requested...");
}

void Renderer::ThreadImpl() {
    while (isRunning) {
        Render();

        // If the game is not focused, do not process state changes,
        // or will start focusing game & overlay
        if (this->isFocused && HandleState())
            continue; // It will cause flickering if we handle window order after window closes

        HandleWindowOrder();
    }

    // Once exited, destroy everything
    Window::DestroyImGui();
    Window::DestroyDevice();
    Window::DespawnWindow();
}

void Renderer::Render() {
    Window::StartRender();

    Esp::Render();
    Overlays::Render();

    Menu::RenderStartupHelp();
    if (isOpen) {
        Menu::Render();
#ifndef _WIN32
        // While the menu is open, input must be captured not just over the
        // menu but also over the movable / resizable overlay widgets (radar,
        // bomb timer, spectator list, velocity graph). The XShape input
        // region below is the ONLY interactive area of the always
        // click-through overlay, so without these rects the widgets can
        // never be grabbed, dragged or resized. Each is inflated so a drag
        // that drifts past its edge without releasing stays captured.
        float cap[4];
        ComputeCaptureUnion(cap);
        Window::SetMenuCapture(true, cap[0], cap[1], cap[2], cap[3]);
#endif
    } else {
#ifndef _WIN32
        Window::SetMenuCapture(false, 0.f, 0.f, 0.f, 0.f);
#endif
    }

    Window::EndRender();
}

bool Renderer::HandleState() {
    isRunning = Window::shouldRun; // From the window event handler

    static bool was_holding = false;

#ifdef _WIN32
    bool pressed_insert = (GetAsyncKeyState(VK_INSERT) & 0x8000);
    bool pressed_rshift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

    bool pressed_end = (GetAsyncKeyState(VK_END) & 0x8000);
#else
    bool pressed_insert = Window::IsKeyDown(XK_Insert);
    // Right Shift is deliberately NOT a toggle key on Linux: the overlay is
    // always keyboard-transparent, so the key also reaches CS2, where it is
    // the walk key. Toggling on it would close the menu every time the player
    // walks. Insert only.
    bool pressed_rshift = false;
    bool pressed_end = Window::IsKeyDown(XK_End);
#endif

    bool should_toggle = !was_holding && (pressed_insert || pressed_rshift);

    if (should_toggle || pressed_end) { // Toggle when pressing end to trigger the config save :v
        this->isOpen = !isOpen;

        // Release cursor when opening the menu
        // Sometimes flashes the render as its handling the window order
#ifdef _WIN32
        if (this->isOpen)
            SetForegroundWindow(Window::hwnd);
        else
            SetForegroundWindow(Engine::GetProcess()->hwnd_);
#endif

        Window::SetClickthrough(Window::hwnd, !this->isOpen);
        LOGF(VERBOSE, "Toggling menu state to {}", this->isOpen);

        // Not the best way, but wont bother the user
        // As far as i know, no one has complained about the config saving system :D
        std::thread(Config::Write).detach(); // Not needed, but just in case
    }

    if (pressed_end)
        this->isRunning = false;

    was_holding = pressed_insert || pressed_rshift;
    return should_toggle;
}

bool Renderer::HandleWindowOrder() {
    auto p = Engine::GetProcess();

#ifdef _WIN32
    if (!p || (!p->hwnd_ && !p->UpdateHWND()))
        return false;

    // Check if game window is still valid, if not, most likely game closed
    if (!IsWindow(p->hwnd_))
        this->isRunning = false;

    static bool overlay_visible = true;
    auto foreground = GetForegroundWindow();
    this->isFocused = (foreground == Window::hwnd || foreground == p->hwnd_);

    if (!this->isFocused && overlay_visible) {
        LOGF(VERBOSE, "Hiding overlay window because the game is not focused");
        ShowWindow(Window::hwnd, SW_HIDE);
        overlay_visible = false;
        return true;
    }

    if (!overlay_visible && this->isFocused) {  
        LOGF(VERBOSE, "Showing overlay window as the game is focused");
        ShowWindow(Window::hwnd, SW_SHOW);
        overlay_visible = true;
        return true;
    }

    static RECT last_rect = { 0, 0, 0, 0 };

    RECT window_rect;
    if (!GetWindowRect(p->hwnd_, &window_rect))
        return false;

    // All good, no movements from the client
    if (memcmp(&window_rect, &last_rect, sizeof(RECT)) == 0)
        return true;

    RECT client_rect;
    if (!GetClientRect(p->hwnd_, &client_rect))
        return false;

    POINT top_left = { client_rect.left, client_rect.top };
    POINT bottom_right = { client_rect.right, client_rect.bottom };

    ClientToScreen(p->hwnd_, &top_left);
    ClientToScreen(p->hwnd_, &bottom_right);

    RECT screen_rect = { top_left.x, top_left.y, bottom_right.x, bottom_right.y };

    SetWindowPos(
        Window::hwnd,
        HWND_TOPMOST,
        screen_rect.left,
        screen_rect.top,
        screen_rect.right - screen_rect.left,
        screen_rect.bottom - screen_rect.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );

    last_rect = window_rect;

    return true;
#else
    if (!p || !p->pid_ || !std::filesystem::exists("/proc/" + std::to_string(p->pid_))) {
        this->isRunning = false;
        return false;
    }

    // Check if CS2 is the focused window via hyprctl
    static bool overlay_visible = true;
    {
        FILE* pipe = ::popen("hyprctl activewindow -j", "r");
        if (pipe) {
            char buf[512];
            std::string json;
            size_t n;
            while ((n = ::fread(buf, 1, sizeof(buf), pipe)) > 0)
                json.append(buf, n);
            ::pclose(pipe);

            bool cs2_focused = false;
            bool have_geo = false;
            int gx = 0, gy = 0, gw = 0, gh = 0;
            try {
                auto j = nlohmann::json::parse(json);
                if (j.contains("pid") && j["pid"].is_number()) {
                    const pid_t active = j["pid"].get<pid_t>();
                    // Hyprland defaults to focus-follows-mouse, so hovering the
                    // overlay (menu open) focuses OUR window, not the game's.
                    // Treat that as in-game or the overlay hides/refocuses in a
                    // loop every time the mouse crosses it.
                    cs2_focused = (active == p->pid_ || active == ::getpid());
                }
                // Hyprland reports the focused window's geometry in layout
                // coordinates; the overlay must match it exactly so the ESP
                // (projected at the game's render size) lines up.
                if (j.contains("at") && j["at"].is_array() && j["at"].size() >= 2 &&
                    j.contains("size") && j["size"].is_array() && j["size"].size() >= 2) {
                    gx = j["at"][0].get<int>();
                    gy = j["at"][1].get<int>();
                    gw = j["size"][0].get<int>();
                    gh = j["size"][1].get<int>();
                    have_geo = true;
                }
            } catch (...) {}

            this->isFocused = cs2_focused;

            if (this->isFocused) {
                if (!overlay_visible) {
                    Window::SetVisible(true);
                    overlay_visible = true;
                }
                // Keep the overlay glued to the game window while focused.
                if (have_geo)
                    Window::TrackGameWindow(gx, gy, gw, gh);
                return true;
            }

            if (overlay_visible) {
                Window::SetVisible(false);
                overlay_visible = false;
            }
            return true;
        } else {
            this->isFocused = true; // fallback: assume focused
        }
    }

    return true;
#endif
}
