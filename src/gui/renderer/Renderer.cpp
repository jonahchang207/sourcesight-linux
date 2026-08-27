#include "Renderer.hpp"
#include "window/Window.hpp"

#include "core/engine/Engine.hpp"
#include "gui/frontend/esp/Esp.hpp"
#include "gui/frontend/menu/Menu.hpp"
#include "gui/frontend/overlays/Overlays.hpp"
#ifndef _WIN32
#include <X11/keysym.h>
#include <filesystem>
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
        LOGF(FATAL, "Failed to create window");
        return false;
    }

    if (!Window::CreateDevice()) {
        LOGF(FATAL, "Failed to create device");
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
        // Keep the menu clickable; everything outside it stays click-through.
        Window::SetMenuCapture(true, Menu::GetPos().x, Menu::GetPos().y, Menu::GetSize().x, Menu::GetSize().y);
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
            try {
                auto j = nlohmann::json::parse(json);
                if (j.contains("pid") && j["pid"].is_number())
                    cs2_focused = (j["pid"].get<pid_t>() == p->pid_);
            } catch (...) {}

            this->isFocused = cs2_focused;
        } else {
            this->isFocused = true; // fallback: assume focused
        }
    }

    if (!this->isFocused && overlay_visible) {
        Window::SetVisible(false);
        overlay_visible = false;
        return true;
    }

    if (!overlay_visible && this->isFocused) {
        Window::SetVisible(true);
        overlay_visible = true;
        return true;
    }

    return true;
#endif
}
