#include "Input/Internals/InputSample.h"
#include <algorithm>

bool IsInputNeutral(const InputSample& sample) {
    return !std::ranges::any_of(sample.keysDown, [](bool down) { return down; }) &&
           !std::ranges::any_of(sample.keysPressed, [](bool pressed) { return pressed; }) &&
           !std::ranges::any_of(sample.keysReleased, [](bool released) { return released; }) &&
           sample.touches.empty() && !sample.mouseDown && !sample.mousePressed && !sample.mouseReleased;
}
