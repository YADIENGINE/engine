#pragma once
#include <vector>
#include "Input/Input.h"
#include "Utilities/Color4.h"

// One player's keys in menus, and the colour a look may give their cursor. Alpha 0 uses the theme's accent.
struct UiKeys {
    std::vector<InputKey> up;
    std::vector<InputKey> down;
    std::vector<InputKey> left;
    std::vector<InputKey> right;
    std::vector<InputKey> confirm;
    std::vector<InputKey> back;
    std::vector<InputKey> previous; // Page or tab, wherever the cursor is
    std::vector<InputKey> next;
    Color4 color{0, 0, 0, 0};

    // Arrows and WASD, Enter and Space, Escape, Q and E: one player on the whole keyboard.
    [[nodiscard]] static UiKeys All();
    // Two players side by side: WASD with Space, Q and E; arrows with Enter, Page Up and Page Down. Escape is shared.
    [[nodiscard]] static UiKeys Wasd(Color4 color = {0, 0, 0, 0});
    [[nodiscard]] static UiKeys Arrows(Color4 color = {0, 0, 0, 0});
};
