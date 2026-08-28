#pragma once

// Triggerbot: automatically fires when the crosshair is on an enemy.
// Detects enemies via memory reads and injects clicks through the kernel
// mouse device (/dev/person-mouse) or XTest.

class Triggerbot {
public:
    static void Init();
    static void Update();

    // True when the crosshair is currently over an enemy (uses visible_only,
    // target_part and threshold). Public so the spinbot can fire while
    // spinning without duplicating the projection logic.
    static bool OnEnemy();

    // Inject one left-click through XTest. Returns true when a click was
    // actually injected (X display available).
    static bool Fire();

    // Linked aim+trigger: called right after MouseAim::Update. When the
    // locked target has been reached, releases the strafe keys (A/D) and
    // fires triggerbot bursts until the target is lost.
    static void UpdateAimLink();

private:
    static void UpdateImpl();
    static bool IsCrosshairOnEnemy();

    // One burst cycle using the configured burst_count / burst delay / delay.
    static void Burst();
};
