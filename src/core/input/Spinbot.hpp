#pragma once

// Spinbot: continuously spins the view (and, with it, the player model) by
// driving the kernel mouse device in a circle — no view-angle writes, no
// game-memory access. Rotating the model makes incoming shots fan out across
// a moving hitbox.
//
// Disables via the F9 panic key and pauses while the menu overlay is open so
// it never fights your own mouse while configuring.

class Spinbot {
public:
    static void Init();
    static void Update();

private:
    static void UpdateImpl();
};