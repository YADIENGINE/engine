#include "Input/Internals/InputNormalizer.h"
#include <algorithm>
#include <utility>

void InputNormalizer::Cancel() { _suppressUntilNeutral = true; }

DeviceInput InputNormalizer::Read(const InputSample& sample,
                                  const std::function<std::optional<Vec2>(Float2)>& mapPoint) {
    DeviceInput input;
    input.keysPressed = sample.keysPressed;
    input.keysDown = sample.keysDown;
    input.mousePosition = mapPoint(sample.mousePosition);
    input.mousePressed = sample.mousePressed;
    input.mouseReleased = sample.mouseReleased;
    input.mouseDown = sample.mouseDown;
    input.wheel = sample.wheel;

    std::vector<int> touchIds;
    touchIds.reserve(sample.touches.size());
    input.touches.reserve(sample.touches.size());
    for (const auto& sampleTouch : sample.touches) {
        const InputTouch touch{sampleTouch.id, mapPoint(sampleTouch.position)};
        input.touches.push_back(touch);
        touchIds.push_back(touch.id);
        if (!touch.position) continue;
        input.heldPoints.push_back(*touch.position);
        if (std::ranges::find(_touchIds, touch.id) == _touchIds.end()) input.pressPoints.push_back(*touch.position);
    }
    _touchIds = std::move(touchIds);
    if (!sample.touches.empty()) _suppressMouse = true;
    if (!_suppressMouse) {
        if (sample.mousePressed) input.mousePress = input.mousePosition;
        if (sample.mouseMoved) input.mouseHover = input.mousePosition;
        if (input.mousePress) input.pressPoints.push_back(*input.mousePress);
        if (sample.mouseDown && input.mousePosition) input.heldPoints.push_back(*input.mousePosition);
    } else if (sample.touches.empty() && !sample.mouseDown && !sample.mousePressed && !sample.mouseReleased) {
        _suppressMouse = false;
    }
    if (_suppressUntilNeutral) {
        _suppressUntilNeutral = !IsInputNeutral(sample);
        return DeviceInput{.cancelled = true};
    }
    return input;
}
