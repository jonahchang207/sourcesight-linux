#pragma once

#ifdef _WIN32
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
using NativeWindow = HWND;
#else
struct GLFWwindow;
using NativeWindow = GLFWwindow*;
#endif

enum class WindowAffinity {
	Disabled,
	Black,
	Invisible,
};

class Window {
public:
	static bool CreateDevice();
	static void DestroyDevice();

	static bool SpawnWindow();
	static void DespawnWindow();

	static bool CreateImGui();
	static void DestroyImGui();

	static void StartRender();
	static void EndRender();

	static bool vsync;
	static NativeWindow hwnd;
#ifdef _WIN32
	static HWND viewport;
	static WNDCLASSEX wc;

	static bool IsWindowInForeground(HWND window) { return GetForegroundWindow() == window; }
	static bool BringToForeground(HWND window) { return SetForegroundWindow(window); }
#endif

	static void SetTopMost(NativeWindow window, bool up_down = true);
	static void SetClickthrough(NativeWindow window, bool clickthrough = true);
#ifdef _WIN32
	static void SetBounds(HWND window, RECT bounds);
	static void SetForeground(HWND window);
#endif
	static bool SetAffinity(NativeWindow window, WindowAffinity afi);
	static void SetVSync(bool enable = false);
#ifdef _WIN32
	static void SetParent(HWND window, HWND parent);

	static ID3D11Device* device;
	static ID3D11DeviceContext* device_context;
	static IDXGISwapChain* swap_chain;
	static ID3D11RenderTargetView* render_targetview;

	inline static RECT bounds;
#else
	static bool IsKeyDown(int key);
	static bool IsFocused();
#endif
	inline static bool renderMenu = false;
	inline static bool shouldRun = true;

};
