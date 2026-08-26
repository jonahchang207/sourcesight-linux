#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

// Kernel-level virtual mouse backed by /dev/person-mouse.
//
// Logic ported from the CS2AiAimbot/Nod project (kmouse.py): the
// ``person_mouse`` kernel module exposes a character device that accepts
// relative cursor writes and injects them into the kernel input subsystem,
// bypassing the compositor (X11/Wayland) entirely. Games that grab raw input
// (e.g. CS2) honour relative deltas and the OS cursor advances by the same
// amount, so this works everywhere.
//
//   write "dx dy\n"  → relative movement (positive x = right, positive y = down)
//   write "click\n"  → left-button press and release
//   ioctl 0x4008AA01 → set virtual screen bounds (struct pm_size)
class KernelMouse {
public:
    KernelMouse() = default;
    ~KernelMouse();

    KernelMouse(const KernelMouse&) = delete;
    KernelMouse& operator=(const KernelMouse&) = delete;

    // Open /dev/person-mouse (idempotent) and report the virtual screen size.
    // Returns true when the device is usable.
    bool Open(int width, int height);
    void Close();

    bool Available() const;

    // Inject a RELATIVE movement of (dx, dy) mouse counts.
    bool Move(int dx, int dy);

    // Inject one left-button press and release at the cursor's position.
    bool LeftClick();

    // Move to an absolute screen point using an exact relative delta: read
    // the compositor cursor position, apply a per-axis deadzone (pixels),
    // then send only the remaining delta.
    bool MoveTo(float x, float y, float deadzone = 0.0f);

    // Query the current Hyprland cursor position in desktop coordinates.
    // Returns std::nullopt when the compositor cannot be queried.
    static std::optional<std::pair<int, int>> CursorPosition();

private:
    int fd_ = -1;
    std::mutex write_mutex_;
};
