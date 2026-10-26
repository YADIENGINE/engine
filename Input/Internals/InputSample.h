#pragma once
#include <array>
#include <vector>
#include "Input/Input.hpp"
#include "Utilities/Float2.h"

struct InputSampleTouch {
    int id = 0;
    Float2 position;
};

// Backend sample in window coordinates. Never delivered to scenes or serialized as public input.
struct InputSample {
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysPressed{};
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysDown{};
    Float2 mousePosition;
    bool mouseMoved = false;
    bool mousePressed = false;
    bool mouseReleased = false;
    bool mouseDown = false;
    float wheel = 0.0f;
    std::vector<InputSampleTouch> touches;
};

[[nodiscard]] bool IsInputNeutral(const InputSample& sample);
