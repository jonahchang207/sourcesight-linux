#pragma once

#include <mutex>

#include "core/input/KernelMouse.hpp"

// Cursor controller that drives the kernel mouse toward a target screen
// point.  Reads the enemy's head bone position from memory, projects it to
// screen coordinates, and sends the mouse there directly — speed-capped for
// a smooth glide.  Target selection locks on one enemy and sticks until the
// lock breaks (death, off-screen, FOV exit), so there is no switching jitter.
//
// Efficiency notes:
//   * Cursor position is tracked locally from sent deltas; hyprctl is only
//     re-queried ~4x/s to absorb external mouse movement.
//   * FOV gating keeps corrections bounded to the visible acquisition ring.
class MouseAim {
public:
    // Advance one engine tick: select a target, compute the correction for
    // the configured algorithm, and inject it through the kernel mouse.
    static void Update();

    // Feed an explicit target in desktop screen coordinates (0..screen_w/h).
    static void SetTarget(float x, float y);
    static void ClearTarget();

    static bool Available();

    // True when /dev/person-mouse exists as a character device, regardless of
    // whether this process has it open. Used for the menu status line.
    static bool DriverInstalled();

    // Current tracked cursor estimate (desktop mode). Returns false when the
    // cursor has not been observed yet or game mode is active.
    static bool TrackedCursor(float& x, float& y);

    // Monitor size the aim controller was configured with.
    static void ScreenSize(float& width, float& height);

    // Aim reference point: the tracked cursor (desktop) or the screen centre
    // (pointer-locked games). Used by the colour scanner for its scan region.
    static void ReferencePoint(float& x, float& y);

    // Current locked target in screen coordinates and whether it is in line
    // of sight. Returns false when the aim has no target right now.
    static bool TargetInfo(float& x, float& y, bool& visible);

private:
    MouseAim() = default;

    static void Init();
    static void MaybeResyncCursor(double now);
    static bool SelectNearestEnemy();
    static int ValidateTarget();

    // Unlocked variants for internal use while Update() holds mtx_.
    static void SetTargetUnlocked(float x, float y);
    static void ClearTargetUnlocked();

    static KernelMouse& Mouse();

    static std::mutex mtx_;
    static bool inited_;
    static float screen_w_;
    static float screen_h_;

    // Tracked cursor estimate (desktop mode). Re-synced from hyprctl on a
    // fixed cadence to absorb physical mouse movement.
    static bool cursor_tracked_;
    static float cursor_x_;
    static float cursor_y_;
    static double last_resync_;

    static bool has_target_;
    static float target_x_;
    static float target_y_;

    // True while the current target came from the auto enemy selection;
    // those targets are re-validated each tick and cleared on lock-break.
    static bool auto_target_;

    // Which player the lock is currently on, and whether their head is in
    // our line of sight. Shown as a cyan (hidden) / magenta (visible) marker.
    static int locked_player_index_;
    static int prev_locked_index_;
    static bool target_visible_;

    static double last_update_;
};
