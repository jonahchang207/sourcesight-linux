#pragma once

class Macro {
public:
    // Runs every engine tick and fires any enabled macros based on the
    // current game state and global input state.
    static void Update();
private:
    static void UpdateImpl();
};
