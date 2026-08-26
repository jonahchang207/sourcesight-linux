#ifndef _WIN32
#include "Window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#define Window X11Window
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#undef Window
#include <GL/gl.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <cstring>

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

void PollGlobalKeyboard() {
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
// When the menu is open, the input region covers the whole screen: every click
// goes to the overlay and ImGui routes it to the menu, its popups, and the
// draggable overlay panels (radar, spectator list). Nothing falls through to
// the game while configuring. When the menu is closed the region is empty, so
// the game receives every click again.
void ApplyClickThrough() {
    Display* display = glfwGetX11Display();
    if (!display || !Window::hwnd)
        return;

    X11Window xwin = glfwGetX11Window(Window::hwnd);

    if (Window::capture_menu) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(Window::hwnd, &width, &height);
        if (width <= 0 || height <= 0)
            return;
        XRectangle rect = { 0, 0, static_cast<unsigned short>(width), static_cast<unsigned short>(height) };
        XShapeCombineRectangles(display, xwin, ShapeInput, 0, 0, &rect, 1, ShapeSet, Unsorted);
    } else {
        XShapeCombineRectangles(display, xwin, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    }
}

} // namespace

bool Window::vsync = false;
NativeWindow Window::hwnd = nullptr;

bool Window::SpawnWindow() {
    // Omarchy uses Hyprland. GLFW's X11 backend gives us reliable transparent
    // and mouse-passthrough window controls under XWayland.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) return false;
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
    hwnd = glfwCreateWindow(mode->width, mode->height, "SourceSight Linux", nullptr, nullptr);
    if (!hwnd) {
        glfwTerminate();
        return false;
    }
    glfwSetWindowPos(hwnd, 0, 0);
    glfwSetWindowAttrib(hwnd, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    ApplyClickThrough();
    glfwMakeContextCurrent(hwnd);
    glfwSwapInterval(0);
    return true;
}

void Window::DespawnWindow() {
    if (hwnd) glfwDestroyWindow(hwnd);
    hwnd = nullptr;
    glfwTerminate();
}

bool Window::CreateDevice() { return hwnd != nullptr; }
void Window::DestroyDevice() {}

bool Window::CreateImGui() {
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

void Window::SetTopMost(NativeWindow, bool) {}
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
    if (affinity != WindowAffinity::Disabled)
        LOGF(WARNING, "Streamproof capture affinity is not available under Hyprland");
    return affinity == WindowAffinity::Disabled;
}
void Window::SetVSync(bool enable) {
    vsync = enable;
    glfwSwapInterval(enable ? 1 : 0);
}
bool Window::IsKeyDown(int keysym) {
    Display* display = glfwGetX11Display();
    if (!display) return false;
    char keys[32]{};
    XQueryKeymap(display, keys);
    const KeyCode code = XKeysymToKeycode(display, static_cast<KeySym>(keysym));
    return code && (keys[code / 8] & (1 << (code % 8)));
}
bool Window::IsFocused() { return hwnd && glfwGetWindowAttrib(hwnd, GLFW_FOCUSED); }
#endif
