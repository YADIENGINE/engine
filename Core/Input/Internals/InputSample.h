#pragma once
#include <array>
#include <vector>
#include "Input/Input.h"
#include "Utilities/Geometry/Float2.h"

struct InputSampleTouch {
    int id = 0;
    Float2 position;
};

// Backend sample in window coordinates. Never delivered to scenes or serialized as public input.
struct InputSample {
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysPressed{};
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysDown{};
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysReleased{};
    Float2 mousePosition;
    bool mouseMoved = false;
    // The left button. These fields decide it; the MouseLeft key slots are ignored (set from them when normalized).
    bool mousePressed = false;
    bool mouseReleased = false;
    bool mouseDown = false;
    float wheel = 0.0f;
    std::vector<InputSampleTouch> touches;
};

[[nodiscard]] bool IsInputNeutral(const InputSample& sample);
