#pragma once

// Triggerbot: automatically fires when the crosshair is on an enemy.
// Detects enemies via memory reads and injects clicks through the kernel
// mouse device (/dev/person-mouse) or XTest.

class Triggerbot {
public:
    static void Init();
    static void Update();

private:
    static void UpdateImpl();
    static bool IsCrosshairOnEnemy();
    static void Fire();
};
