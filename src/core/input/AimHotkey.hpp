#pragma once

#include <atomic>
#include <thread>

// Listens for mouse button 5 (side button) presses and flags a toggle
// request, so the aim can be switched on/off mid-game without opening the
// menu. Ported from the CS2AiAimbot hotkey.py logic.
//
// Two independent sources keep the toggle working everywhere:
//   * evdev (/dev/input/event* — BTN_SIDE/BTN_EXTRA): works even with no
//     X server, but needs read access to the device (input group / root).
//   * XInput2 raw button events (details 5/8/9): needs X11 but NOT device
//     permissions, and, being raw events, they bypass the raw-input grab
//     CS2 takes on the pointer. This is the path that works for unprivileged
//     users.
class AimHotkey {
public:
    AimHotkey() = default;
    ~AimHotkey();

    AimHotkey(const AimHotkey&) = delete;
    AimHotkey& operator=(const AimHotkey&) = delete;

    // Start or stop the listener threads to match the configuration.
    void Sync(bool want_running);

    // True once a side button was pressed since the last call (and clears
    // the flag).
    bool ConsumeToggle();

private:
    void EvdevLoop();
    void X11Loop();

    std::thread evdev_thread_;
    std::thread x11_thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> toggle_{ false };
};
