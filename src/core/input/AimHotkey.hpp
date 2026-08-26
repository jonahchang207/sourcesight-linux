#pragma once

#include <atomic>
#include <thread>

// Listens for mouse button 5 (side button) presses on any connected mouse and
// flags a toggle request, so the aim can be switched on/off mid-game without
// opening the menu. Ported from the CS2AiAimbot hotkey.py logic.
class AimHotkey {
public:
    AimHotkey() = default;
    ~AimHotkey();

    AimHotkey(const AimHotkey&) = delete;
    AimHotkey& operator=(const AimHotkey&) = delete;

    // Start or stop the listener thread to match the configuration.
    void Sync(bool want_running);

    // True once MB5 was pressed since the last call (and clears the flag).
    bool ConsumeToggle();

private:
    void Loop();

    std::thread thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> toggle_{ false };
};
