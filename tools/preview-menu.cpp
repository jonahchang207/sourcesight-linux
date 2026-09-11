#include "common.hpp"
#include "gui/frontend/menu/Menu.hpp"

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {

struct Options {
    bool headless = false;
    int width = 1280;
    int height = 900;
    std::string screenshot;
};

Options Parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--headless") {
            options.headless = true;
        } else if (arg == "--width" && i + 1 < argc) {
            options.width = std::max(940, std::atoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            options.height = std::max(600, std::atoi(argv[++i]));
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else {
            throw std::runtime_error("usage: preview-menu [--headless] [--width N] [--height N] [--screenshot FILE]");
        }
    }
    return options;
}

void RenderFrame() {
    ImGui::NewFrame();
    Menu::Render();
    ImGui::Render();
}

void SelectTab(int tab, const ImVec2& menu_pos) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(menu_pos.x + 80.0f, menu_pos.y + 155.0f + tab * 52.0f);
    io.AddMouseButtonEvent(0, true);
    RenderFrame();
    io.AddMouseButtonEvent(0, false);
    RenderFrame();
}

void WritePpm(const std::string& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("unable to open screenshot destination");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        output.write(reinterpret_cast<const char*>(pixels.data() + static_cast<size_t>(y) * width * 3), width * 3);
}

void RunHeadless(const Options& options) {
    const EGLDisplay display=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
    if(display==EGL_NO_DISPLAY || !eglInitialize(display,nullptr,nullptr) || !eglBindAPI(EGL_OPENGL_API))
        throw std::runtime_error("surfaceless EGL initialization failed");
    const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
    EGLConfig config{}; EGLint count{};
    if(!eglChooseConfig(display,attributes,&config,1,&count) || count!=1)
        throw std::runtime_error("no suitable EGL framebuffer");
    const EGLint surface_attributes[]={EGL_WIDTH,options.width,EGL_HEIGHT,options.height,EGL_NONE};
    const auto surface=eglCreatePbufferSurface(display,config,surface_attributes);
    const auto context=eglCreateContext(display,config,EGL_NO_CONTEXT,nullptr);
    if(surface==EGL_NO_SURFACE || context==EGL_NO_CONTEXT || !eglMakeCurrent(display,surface,surface,context))
        throw std::runtime_error("EGL context creation failed");
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(static_cast<float>(options.width), static_cast<float>(options.height));
    io.DeltaTime = 1.0f / 60.0f;
    Menu::SetPreviewMode(true);
    if (!Menu::Init()) throw std::runtime_error("menu initialization failed");
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui_ImplOpenGL3_NewFrame();
    RenderFrame();
    for (int tab = 0; tab < 7; ++tab)
        SelectTab(tab, Menu::GetPos());
    // Auto-resizing children settle after the tab-switch frame.
    for(int frame=0;frame<3;++frame) RenderFrame();
    glViewport(0,0,options.width,options.height);
    glClearColor(.025f,.030f,.040f,1.f);glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if(!options.screenshot.empty()) WritePpm(options.screenshot,options.width,options.height);
    std::cout << "offline menu preview rendered at " << options.width << 'x' << options.height << '\n';
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
}

void RunVisible(const Options& options) {
    if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "SourceSight menu preview", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("GLFW window creation failed");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    Menu::SetPreviewMode(true);
    if (!Menu::Init()) throw std::runtime_error("menu initialization failed");

    bool screenshot_written = false;
    int frames=0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        RenderFrame();
        int framebuffer_width = 0, framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.025f, 0.030f, 0.040f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (++frames>=3 && !screenshot_written && !options.screenshot.empty()) {
            WritePpm(options.screenshot, framebuffer_width, framebuffer_height);
            screenshot_written = true;
            std::cout << "wrote preview screenshot: " << options.screenshot << '\n';
        }
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace

int main(int argc, char** argv) {
    LogHelper::Init();
    try {
        const Options options = Parse(argc, argv);
        if (options.headless)
            RunHeadless(options);
        else
            RunVisible(options);
        LogHelper::Destroy();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "preview-menu: " << error.what() << '\n';
        LogHelper::Destroy();
        return 2;
    }
}
