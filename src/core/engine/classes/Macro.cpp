#include "Macro.hpp"

#include "common.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/types/Weapons.hpp"
#include "gui/renderer/Renderer.hpp" // Menu-open guard

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#endif

void Macro::Update() {
    return UpdateImpl();
}

namespace {

#ifdef _WIN32
void TapKey(unsigned char vk) {
    keybd_event(vk, 0, 0, 0);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
}
#else
void TapKey(Display* display, KeySym sym) {
    const KeyCode code = XKeysymToKeycode(display, sym);
    if (!code)
        return;
    XTestFakeKeyEvent(display, code, True, CurrentTime);
    XFlush(display);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    XTestFakeKeyEvent(display, code, False, CurrentTime);
    XFlush(display);
}
#endif

} // namespace

void Macro::UpdateImpl() {
    // Nothing to do unless the overlay is on and at least one macro is enabled.
    if (!cfg::enabled || !cfg::macro::awp_quickswitch)
        return;

    // While the menu is open the user is configuring, not playing: don't inject
    // keys that would switch the in-game weapon mid-click.
    if (Renderer::IsOpen())
        return;

    const auto& local = Cache::Get().local;
    if (!local.alive || local.weapon.item_index != weapon_awp)
        return;

#ifdef _WIN32
    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#else
    // XQueryPointer works even while CS2 holds its pointer grab in-game.
    static Display* display = nullptr;
    if (!display)
        display = XOpenDisplay(nullptr);
    if (!display)
        return;

    Window root_ret, child_ret;
    int root_x = 0, root_y = 0, win_x = 0, win_y = 0;
    unsigned int mask = 0;
    XQueryPointer(display, DefaultRootWindow(display), &root_ret, &child_ret,
                  &root_x, &root_y, &win_x, &win_y, &mask);
    const bool lmb = (mask & Button1Mask) != 0;
#endif

    // Trigger on the rising edge of LMB only, so a held button does not spam
    // the sequence on every engine tick.
    static bool prev_lmb = false;
    const bool rising = lmb && !prev_lmb;
    prev_lmb = lmb;

    if (!rising)
        return;

    LOGF(VERBOSE, "AWP quickswitch macro triggered");

    // Quickswitch: pull out the knife slot (3), wait, then back to primary (1).
    // This cancels the bolt animation after firing the AWP.
#ifdef _WIN32
    TapKey('3');
    std::this_thread::sleep_for(std::chrono::milliseconds(cfg::macro::delay_ms));
    TapKey('1');
#else
    TapKey(display, XK_3);
    std::this_thread::sleep_for(std::chrono::milliseconds(cfg::macro::delay_ms));
    TapKey(display, XK_1);
#endif
}
