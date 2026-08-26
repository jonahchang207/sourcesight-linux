#ifndef _WIN32
#include "Window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#define Window X11Window
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#undef Window
#include <GL/gl.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

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
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (!mode) return false;
    hwnd = glfwCreateWindow(mode->width, mode->height, "SourceSight Linux", nullptr, nullptr);
    if (!hwnd) return false;
    glfwSetWindowPos(hwnd, 0, 0);
    glfwSetWindowAttrib(hwnd, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
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
    return ImGui_ImplGlfw_InitForOpenGL(hwnd, true) && ImGui_ImplOpenGL3_Init("#version 330");
}

void Window::DestroyImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Window::StartRender() {
    glfwPollEvents();
    shouldRun = hwnd && !glfwWindowShouldClose(hwnd);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
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
void Window::SetClickthrough(NativeWindow window, bool enabled) {
    if (window) glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, enabled ? GLFW_TRUE : GLFW_FALSE);
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
