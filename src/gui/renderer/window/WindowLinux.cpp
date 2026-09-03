#ifndef _WIN32
#include "Window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#define Window X11Window
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#undef Window
#include <GL/gl.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

// The overlay window never has OS focus and is always mouse-passthrough, so
// the GLFW backend delivers no input at all. Poll the global X11 state every
// frame instead and feed it to ImGui; this keeps the keyboard-driven menu
// usable while every click and key still reaches CS2.

// X11 keycode -> GLFW key, built once from the current keymap. Keycodes are
// physical positions, so the layout never invalidates this map.
int g_keycode_to_glfw[256];
bool g_keymap_built = false;

void BuildKeycodeMap() {
    if (g_keymap_built)
        return;
    g_keymap_built = true;

    std::memset(g_keycode_to_glfw, -1, sizeof(g_keycode_to_glfw));
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
        const int scancode = glfwGetKeyScancode(key);
        if (scancode > 0 && scancode < 256)
            g_keycode_to_glfw[scancode] = key;
    }
}

bool IsKeysymDown(const char keys[32], Display* display, KeySym sym) {
    const KeyCode code = XKeysymToKeycode(display, sym);
    return code && (keys[code / 8] & (1 << (code % 8)));
}

void FeedModifiers(ImGuiIO& io, Display* display, const char keys[32]) {
    io.AddKeyEvent(ImGuiMod_Shift, IsKeysymDown(keys, display, XK_Shift_L) || IsKeysymDown(keys, display, XK_Shift_R));
    io.AddKeyEvent(ImGuiMod_Ctrl,  IsKeysymDown(keys, display, XK_Control_L) || IsKeysymDown(keys, display, XK_Control_R));
    io.AddKeyEvent(ImGuiMod_Alt,   IsKeysymDown(keys, display, XK_Alt_L) || IsKeysymDown(keys, display, XK_Alt_R));
    io.AddKeyEvent(ImGuiMod_Super, IsKeysymDown(keys, display, XK_Super_L) || IsKeysymDown(keys, display, XK_Super_R)
                                || IsKeysymDown(keys, display, XK_Meta_L) || IsKeysymDown(keys, display, XK_Meta_R));
}

// Intercept GLFW scroll callback: only feed to ImGui when the menu is open.
// Otherwise the scroll wheel passes through to CS2 (commonly bound to jump).
void ScrollCallbackRedirect(GLFWwindow* window, double xoffset, double yoffset) {
    if (Window::capture_menu)
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}

void PollGlobalKeyboard() {
    if (glfwGetPlatform() != GLFW_PLATFORM_X11)
        return;
    Display* display = glfwGetX11Display();
    if (!display)
        return;
    BuildKeycodeMap();

    static char prev_keys[32] = {};
    char keys[32] = {};
    XQueryKeymap(display, keys);

    for (int i = 0; i < 32; ++i) {
        const char changed = keys[i] ^ prev_keys[i];
        if (!changed)
            continue;
        for (int bit = 0; bit < 8; ++bit) {
            if (!(changed & (1 << bit)))
                continue;
            const int keycode = i * 8 + bit;
            const int glfw_key = g_keycode_to_glfw[keycode];
            if (glfw_key < 0)
                continue;
            const bool down = (keys[i] & (1 << bit)) != 0;
            // Reuse the backend's key callback so its full GLFW key -> ImGuiKey
            // mapping (including letters, digits, F-keys) applies unchanged.
            ImGui_ImplGlfw_KeyCallback(Window::hwnd, glfw_key, keycode, down ? GLFW_PRESS : GLFW_RELEASE, 0);
        }
    }
    std::memcpy(prev_keys, keys, sizeof(keys));

    // The backend key callback refreshes modifiers from the (never focused)
    // window state, which is always "up". Re-apply them from the real keymap.
    FeedModifiers(ImGui::GetIO(), display, keys);
}

void PollGlobalPointer() {
    if (glfwGetPlatform() != GLFW_PLATFORM_X11)
        return;
    Display* display = glfwGetX11Display();
    if (!display || !Window::hwnd)
        return;

    X11Window root_ret = 0, child_ret = 0;
    int root_x = 0, root_y = 0, win_x = 0, win_y = 0;
    unsigned int mask = 0;
    if (!XQueryPointer(display, DefaultRootWindow(display), &root_ret, &child_ret,
                       &root_x, &root_y, &win_x, &win_y, &mask))
        return;

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)root_x, (float)root_y);

    // While playing, CS2 holds a pointer grab and hides the OS cursor, so the
    // overlay never receives real button events and the user cannot see where
    // the pointer is. Two fixes, mirroring the keyboard polling above:
    //  1. If the pointer is not already over our own window (which would mean
    //     real events are delivered natively), show ImGui's cursor so the user
    //     can see it on the menu.
    //  2. Feed the global button state into ImGui when the menu is open and the
    //     pointer is over the menu, so clicks work in-game despite the grab.
    const X11Window our_xwin = glfwGetX11Window(Window::hwnd);
    const bool native_delivery = (child_ret == our_xwin);
    io.MouseDrawCursor = Window::capture_menu && !native_delivery;

    static bool mask_initialized = false;
    static unsigned int last_mask = 0;
    if (!mask_initialized) {
        mask_initialized = true;
        last_mask = mask;
    }
    const unsigned int changed = mask ^ last_mask;
    last_mask = mask;

    // Feed clicks anywhere on screen while the menu is open (no rectangle
    // gate): the color picker popups and the draggable panels (radar,
    // spectator list) sit outside the menu window, and ImGui routes each
    // click to whatever widget is under the pointer.
    if (!changed || !Window::capture_menu || native_delivery)
        return;

    struct { unsigned int bit; int button; } buttons[] = {
        { Button1Mask, 0 }, // left
        { Button3Mask, 1 }, // right
        { Button2Mask, 2 }, // middle
    };
    for (const auto& b : buttons) {
        if (changed & b.bit)
            io.AddMouseButtonEvent(b.button, (mask & b.bit) != 0);
    }
}

// GLFW's GLFW_MOUSE_PASSTHROUGH is unreliable under XWayland: it silently does
// nothing unless its own XSHAPE detection succeeded, which leaves the overlay
// with a full input region that swallows every click. Apply the X11 input
// region directly instead - XWayland translates this into a wl_surface input
// region, so Hyprland routes pointer events to the windows below. Re-applied
// every frame so nothing can override it.
//
// While the menu is open only the menu panel itself is interactive: its
// rectangle becomes the input region and everything else stays click-through,
// so the game keeps receiving the mouse. (ImGui popups that open outside the
// panel lose clicks as a trade-off; previously the whole screen was captured,
// which made the overlay swallow every click.) When the menu is closed the
// region is empty, so the game receives every click again.
void ApplyClickThrough() {
    // The X11 native API is unavailable when GLFW selected Wayland.
    if (glfwGetPlatform() != GLFW_PLATFORM_X11 || !Window::hwnd)
        return;
    Display* display = glfwGetX11Display();
    if (!display)
        return;

    X11Window xwin = glfwGetX11Window(Window::hwnd);

    if (Window::capture_menu) {
        // menu_rect is in window (== ImGui display) coordinates; the overlay is
        // positioned exactly over the game window, so they line up directly.
        const float fx = Window::menu_rect[0], fy = Window::menu_rect[1];
        const float fw = Window::menu_rect[2], fh = Window::menu_rect[3];
        if (fw > 0.f && fh > 0.f) {
            XRectangle rect = {
                static_cast<short>(fx), static_cast<short>(fy),
                static_cast<unsigned short>(fw), static_cast<unsigned short>(fh)
            };
            XShapeCombineRectangles(display, xwin, ShapeInput, 0, 0, &rect, 1, ShapeSet, Unsorted);
            return;
        }
        // No menu geometry yet: fall back to capturing the whole window.
        int width = 0, height = 0;
        glfwGetFramebufferSize(Window::hwnd, &width, &height);
        if (width > 0 && height > 0) {
            XRectangle rect = { 0, 0, static_cast<unsigned short>(width), static_cast<unsigned short>(height) };
            XShapeCombineRectangles(display, xwin, ShapeInput, 0, 0, &rect, 1, ShapeSet, Unsorted);
        }
        return;
    }

    XShapeCombineRectangles(display, xwin, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
}

// Request the window manager keep the overlay above every other window by
// setting the standard _NET_WM_STATE_ABOVE hint. GLFW's FLOATING flag maps to
// the same concept but is unreliable under XWayland/Hyprland, so set it
// explicitly. X11-only (Wayland has no notion of this from the client).
void SetX11AlwaysOnTop() {
    if (glfwGetPlatform() != GLFW_PLATFORM_X11 || !Window::hwnd)
        return;
    Display* display = glfwGetX11Display();
    if (!display)
        return;
    const X11Window xwin = glfwGetX11Window(Window::hwnd);
    const Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    const Atom above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    if (wm_state == None || above == None)
        return;
    Atom above_copy = above; // XChangeProperty may write to the source buffer
    XChangeProperty(display, xwin, wm_state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&above_copy), 1);
    XFlush(display);
}

// Wayland clients cannot set their own position; the compositor is the only
// authority. Find our own Hyprland window address (via `hyprctl clients`) and
// ask Hyprland to move us with `movewindowpixel`. The address is stable for
// the lifetime of the window, so it is cached after the first lookup.
bool GetOwnHyprlandAddress(std::string& out) {
    static std::string cached;
    if (!cached.empty()) {
        out = cached;
        return true;
    }

    FILE* pipe = ::popen("hyprctl clients -j", "r");
    if (!pipe)
        return false;
    std::string json;
    char buf[4096];
    size_t n;
    while ((n = ::fread(buf, 1, sizeof(buf), pipe)) > 0)
        json.append(buf, n);
    ::pclose(pipe);

    // Each client object lists "address" before "pid", so locate our pid and
    // scan backwards for the nearest "address": "0x..." belonging to it.
    const std::string marker = "\"pid\": " + std::to_string(::getpid());
    const size_t p = json.find(marker);
    if (p == std::string::npos)
        return false;
    const std::string addr_key = "\"address\": \"";
    const size_t a = json.rfind(addr_key, p);
    if (a == std::string::npos)
        return false;
    const size_t start = a + addr_key.size();
    const size_t end = json.find('"', start);
    if (end == std::string::npos || end == start)
        return false;
    cached = json.substr(start, end - start);
    out = cached;
    return true;
}

// Set the overlay's X11 WM_CLASS to a stable, distinctive name
// ("SourceSight Linux"). Hyprland class-based window rules match against this,
// so the `sourcesight` no_blur/no_shadow rule (installed into ~/.config/hypr by
// scripts/install-omarchy.sh) can actually find the overlay. Without this,
// GLFW leaves the class as the plain executable name and the rule never fires,
// leaving the overlay blurred/frosted over the game.
void SetX11Class() {
    if (glfwGetPlatform() != GLFW_PLATFORM_X11 || !Window::hwnd)
        return;
    Display* display = glfwGetX11Display();
    if (!display)
        return;
    const X11Window xwin = glfwGetX11Window(Window::hwnd);
    XClassHint* hint = XAllocClassHint();
    if (!hint)
        return;
    hint->res_name  = const_cast<char*>("sourcesight");
    hint->res_class = const_cast<char*>("SourceSight Linux");
    XSetClassHint(display, xwin, hint);
    XFree(hint);
    XFlush(display);
}

// Now that the overlay advertises the stable "SourceSight Linux" class, ask the
// compositor to exempt it from blur/shadow. This keeps the fix entirely inside
// the app, so it works without editing any Hyprland/Omarchy config.
//
// On Hyprland 0.55+ (the Lua config era) the legacy `hyprctl keyword
// windowrulev2 no_blur,address:...` hyprlang path no longer applies effect
// rules reliably, which is why the overlay stayed frosted. The supported
// runtime mechanism is `hyprctl eval` with the same Lua `o.window(...)` rule
// the install script writes to the config. We run that here so the exclusion
// is applied regardless of the user's config. This is harmless on other
// compositors: if `hyprctl` is missing the call simply fails silently.
void ApplyHyprlandNoBlurRule() {
    const std::string cmd =
        "hyprctl eval 'o.window({ class = \"^SourceSight Linux$\" }, "
        "{ no_blur = true, no_shadow = true, no_focus = true, pin = true, float = true })' "
        ">/dev/null 2>&1";
    if (::system(cmd.c_str()) != 0)
        LOGF(WARNING, "[window] runtime no_blur rule could not be applied (hyprctl unavailable?)");
}

} // namespace

bool Window::vsync = false;
NativeWindow Window::hwnd = nullptr;

bool Window::SpawnWindow() {
    // The X11 (XWayland) backend is the fully-supported path: transparent
    // framebuffers, mouse passthrough, window positioning, and the global
    // keyboard/pointer polling that drives the menu all work there. Native
    // Wayland has none of that for an overlay window, so prefer X11
    // automatically and only fall back to Wayland when X11 is unavailable.
    // SOURCESIGHT_GLFW_PLATFORM=x11|wayland overrides the default.
    const char* forced_platform = std::getenv("SOURCESIGHT_GLFW_PLATFORM");
    const bool force_wayland = forced_platform && std::strcmp(forced_platform, "wayland") == 0;
    glfwInitHint(GLFW_PLATFORM, force_wayland ? GLFW_PLATFORM_WAYLAND : GLFW_PLATFORM_X11);

    if (!glfwInit()) {
        // Retry with the other backend before giving up (e.g. no XWayland
        // available, or Wayland chosen but the compositor refused us).
        glfwInitHint(GLFW_PLATFORM, force_wayland ? GLFW_PLATFORM_X11 : GLFW_PLATFORM_WAYLAND);
        if (!glfwInit()) {
            LOGF(WARNING, "GLFW initialization failed; display session is unavailable");
            return false;
        }
        LOGF(WARNING, "[window] preferred backend unavailable; fell back to the other one");
    }
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (!mode) {
        glfwTerminate();
        return false;
    }
    hwnd = glfwCreateWindow(mode->width, mode->height, "SourceSight", nullptr, nullptr);
    if (!hwnd) {
        glfwTerminate();
        return false;
    }
    glfwSetWindowPos(hwnd, 0, 0);
    glfwSetWindowAttrib(hwnd, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    // XShape is only available through the X11 native handle. On Wayland,
    // GLFW's native mouse-passthrough attribute is the supported mechanism.
    ApplyClickThrough();
    glfwMakeContextCurrent(hwnd);
    glfwSwapInterval(0);

    SetX11AlwaysOnTop();

    // Advertise the stable "SourceSight Linux" X11 class so Hyprland's
    // class-based no_blur/no_shadow rules can match the overlay window.
    SetX11Class();

    // Hyprland may tile a new window; an overlay must float above the game.
    // One-shot IPC: once our address is known, ask the compositor to float it.
    // (X11 windows honor the GLFW_FLOATING hint above instead.)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        std::string addr;
        if (GetOwnHyprlandAddress(addr)) {
            const std::string cmd = "hyprctl dispatch setfloating address:" + addr + " >/dev/null 2>&1";
            ::system(cmd.c_str());
        }
    }
    // Apply the blur/shadow exemption from inside the app on both backends so
    // the overlay is never frosted, independent of the user's Hyprland config.
    ApplyHyprlandNoBlurRule();

    LOGF(INFO, "[window] overlay backend={} initial size={}x{}",
        glfwGetPlatform() == GLFW_PLATFORM_WAYLAND ? "wayland" : "x11",
        mode->width, mode->height);
    return true;
}

void Window::DespawnWindow() {
    if (hwnd) {
        glfwDestroyWindow(hwnd);
        hwnd = nullptr;
    }
    glfwTerminate();
}

bool Window::CreateDevice() { return hwnd != nullptr; }
void Window::DestroyDevice() {}

bool Window::CreateImGui() {
    if (!hwnd)
        return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (!ImGui_ImplGlfw_InitForOpenGL(hwnd, true)) {
        ImGui::DestroyContext();
        return false;
    }
    // Replace the default scroll callback so scroll events only reach ImGui
    // when the menu is open.  Otherwise they pass through to CS2.
    glfwSetScrollCallback(hwnd, ScrollCallbackRedirect);
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    return true;
}

void Window::DestroyImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Window::StartRender() {
    glfwPollEvents();
    shouldRun = hwnd && !glfwWindowShouldClose(hwnd);
    // Keep the overlay click-through: re-apply the empty input region every
    // frame so map/resize/compositor events can never restore a full one.
    ApplyClickThrough();
    // Re-assert the always-on-top hint: the WM/compositor can drop it when a
    // new window is focused or a fullscreen game appears.
    SetX11AlwaysOnTop();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    // The overlay never has OS focus, so the GLFW backend delivers no input.
    // Poll the global X11 state so the keyboard-driven menu still works while
    // every key and click reaches CS2.
    PollGlobalKeyboard();
    PollGlobalPointer();
    ImGui::NewFrame();
}

void Window::EndRender() {
    ImGui::Render();
    int width{}, height{};
    glfwGetFramebufferSize(hwnd, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(hwnd);
}

void Window::SetTopMost(NativeWindow, bool) {
    // The window is created with GLFW_FLOATING; also set the X11
    // _NET_WM_STATE_ABOVE hint so the WM keeps it above every window.
    // Wayland has no client-side way to express this.
    SetX11AlwaysOnTop();
}
void Window::SetClickthrough(NativeWindow window, bool) {
    // The overlay is ALWAYS mouse-passthrough on Linux: even while the menu is
    // open, clicks outside the menu must reach CS2. Ignore the toggle argument
    // and apply the input region directly rather than relying on GLFW's
    // passthrough, which is a silent no-op under this XWayland.
    (void)window;
    ApplyClickThrough();
}

void Window::SetMenuCapture(bool enabled, float x, float y, float w, float h) {
    capture_menu = enabled;
    menu_rect[0] = x;
    menu_rect[1] = y;
    menu_rect[2] = w;
    menu_rect[3] = h;
    ApplyClickThrough();
}
bool Window::SetAffinity(NativeWindow, WindowAffinity affinity) {
    static bool limitation_logged = false;
    if (affinity != WindowAffinity::Disabled && !limitation_logged) {
        limitation_logged = true;
        LOGF(INFO, "[window] streamproof capture affinity is unavailable on Linux/Hyprland; continuing without compositor-level exclusion");
    }
    return affinity == WindowAffinity::Disabled;
}
void Window::SetVSync(bool enable) {
    vsync = enable;
    glfwSwapInterval(enable ? 1 : 0);
}
bool Window::TrackGameWindow(int x, int y, int w, int h) {
    if (!hwnd || w <= 0 || h <= 0)
        return false;

    // Apply sizes/positions only when they actually change; each call may
    // otherwise shell out to hyprctl, which is comparatively expensive.
    static int last_x = 0, last_y = 0, last_w = 0, last_h = 0;
    const bool size_changed = (w != last_w || h != last_h);
    const bool pos_changed = (x != last_x || y != last_y);

    static bool logged = false;
    if (!logged && (size_changed || pos_changed)) {
        logged = true;
        LOGF(INFO, "[window] tracking game window: at=({}, {}) size={}x{}", x, y, w, h);
    }

    if (size_changed) {
        glfwSetWindowSize(hwnd, w, h);
        last_w = w;
        last_h = h;
    }

    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        if (pos_changed) {
            glfwSetWindowPos(hwnd, x, y);
            last_x = x;
            last_y = y;
        }
        return true;
    }

    // Wayland: ask the compositor to move us to the game window's position.
    if (pos_changed) {
        std::string addr;
        if (GetOwnHyprlandAddress(addr)) {
            const std::string cmd = "hyprctl dispatch movewindowpixel exact " +
                std::to_string(x) + " " + std::to_string(y) +
                ",address:" + addr + " >/dev/null 2>&1";
            if (::system(cmd.c_str()) == 0) {
                last_x = x;
                last_y = y;
            }
        }
    }
    return true;
}

bool Window::IsKeyDown(int keysym) {
    if (glfwGetPlatform() != GLFW_PLATFORM_X11)
        return false;
    Display* display = glfwGetX11Display();
    if (!display) return false;
    char keys[32]{};
    XQueryKeymap(display, keys);
    const KeyCode code = XKeysymToKeycode(display, static_cast<KeySym>(keysym));
    return code && (keys[code / 8] & (1 << (code % 8)));
}
bool Window::IsFocused() { return hwnd && glfwGetWindowAttrib(hwnd, GLFW_FOCUSED); }

void Window::SetVisible(bool visible) {
    if (!hwnd) return;
    if (visible)
        glfwShowWindow(hwnd);
    else
        glfwHideWindow(hwnd);
}
#endif
