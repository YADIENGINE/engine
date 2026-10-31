#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <vector>
#include "Utilities/Vec2.h"

// clang-format off
enum class InputKey {
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Zero, One, Two, Three, Four, Five, Six, Seven, Eight, Nine,
    Up, Down, Left, Right,
    Enter, Space, Escape,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
    Tab, Backspace, Delete, Insert,
    Home, End, PageUp, PageDown,
    Minus, Equal, Subtract, Add,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count
};
// clang-format on

struct InputTouch {
    int id = 0;
    std::optional<Vec2> position; // Touch identity is preserved outside the canvas too.
};

// One frame of normalized input. Public positions are logical canvas coordinates.
// Scenes borrow this value as const; hosts and tests can construct it without raylib.
struct DeviceInput {
    // Contact/focus loss: actions in this snapshot are skipped and pending gestures are cancelled.
    bool cancelled = false;
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysPressed{};
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysDown{};
    std::optional<Vec2> mousePosition;
    bool mousePressed = false;
    bool mouseReleased = false;
    bool mouseDown = false;
    std::optional<Vec2> mousePress; // Eligible left-button press; touch-generated mouse events are suppressed.
    std::optional<Vec2> mouseHover; // Eligible mouse movement, not a stationary hover that steals keyboard focus.
    float wheel = 0.0f;             // Mouse wheel movement this frame (positive: up).
    std::vector<InputTouch> touches;
    // Contact beginnings, not release taps. Includes independent new fingers, or an eligible mouse press.
    std::vector<Vec2> pressPoints;
    // Current valid finger positions, or the held mouse when no touch owns input.
    std::vector<Vec2> heldPoints;

    [[nodiscard]] constexpr bool Pressed(InputKey key) const { return keysPressed[static_cast<std::size_t>(key)]; }
    [[nodiscard]] constexpr bool Down(InputKey key) const { return keysDown[static_cast<std::size_t>(key)]; }
    [[nodiscard]] constexpr bool ShiftDown() const { return Down(InputKey::LeftShift) || Down(InputKey::RightShift); }
};
