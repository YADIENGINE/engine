#pragma once
#include <cstddef>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// What a control hint shows: "press [Space]", "move with [WASD]". Labels are drawn from strokes, so a prompt needs no
// font.
// Wasd and ArrowKeys are the four keys together; MouseWheel and MouseMove show a mouse with arrows.
// clang-format off
enum class KeyPrompt {
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Space, Enter, Escape, Tab, Shift, Ctrl, Alt, Backspace, Delete,
    Up, Down, Left, Right,
    Wasd, ArrowKeys,
    Mouse, MouseLeft, MouseRight, MouseMiddle, MouseWheel, MouseMove,
    Count
};
// clang-format on
inline constexpr std::size_t KeyPromptCount = static_cast<std::size_t>(KeyPrompt::Count);

// How prompts look on their own; a game changes only what it needs.
struct KeyPromptLook {
    Color4 cap{236, 236, 244, 255};  // A key's top; a mouse's body
    Color4 side{146, 146, 170, 255}; // A key's front edge; a mouse's outline and seams
    Color4 label{30, 28, 46, 255};
    Color4 accent{255, 150, 60, 255}; // The mouse button or wheel the prompt names, and a mouse's arrows
};

// Width over height of the prompt's drawing, to lay out a row of prompts.
[[nodiscard]] float KeyPromptAspect(KeyPrompt prompt);

// Draws the prompt as large as it fits, centred in 'box', keeping its aspect. 'pressed' (0..1) sinks a key into its
// edge and darkens the accent, for a held key or a pulsing hint.
void DrawKeyPrompt(Painter& paint, const FloatRect& box, KeyPrompt prompt, float pressed = 0.0f,
                   const KeyPromptLook& look = {});
