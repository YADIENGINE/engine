#include "Editor/Internals/Viewport/ViewportInputRouter.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Editor/Internals/Protocol/WorkerProtocol.h"

ViewportInputRouter::ViewportInputRouter() = default;
DeviceInput ViewportInputRouter::Clear() {
    _buttons.fill(false);
    _suppressUntilNeutral = true;
    _lastMouse.reset();
    _touchIds.clear();
    _ownedTouches.clear();
    _normalizer.Cancel();
    return DeviceInput{.cancelled = true};
}
DeviceInput ViewportInputRouter::Route(const InputSample& windowInput, Float2 mouse, const SceneViewport& viewport,
                                       bool windowFocused, bool viewportFocused, bool keyboardBlocked,
                                       bool pointerBlocked) {
    if (!windowFocused || viewport.size.x <= 0 || viewport.size.y <= 0 || viewport.canvas.x <= 0 ||
        viewport.canvas.y <= 0)
        return Clear();
    const auto mapPoint = [&viewport](Float2 point) { return MapViewportPoint(viewport, point); };
    if (_suppressUntilNeutral) {
        _suppressUntilNeutral = !IsInputNeutral(windowInput);
        return _normalizer.Read(windowInput, mapPoint);
    }
    InputSample routed;
    if (viewportFocused && !keyboardBlocked) {
        routed.keysDown = windowInput.keysDown;
        routed.keysPressed = windowInput.keysPressed;
        routed.keysReleased = windowInput.keysReleased;
    }
    const auto position = mapPoint(mouse);
    routed.mousePosition = mouse;
    const bool pointerHere = position && !pointerBlocked;
    if (pointerHere) {
        routed.mouseMoved = position != _lastMouse;
        routed.wheel = windowInput.wheel;
    }
    // Mouse buttons follow the pointer, not keyboard focus: a press over the viewport is the scene's until released.
    for (std::size_t i = 0; i < _buttons.size(); ++i) {
        const auto key = static_cast<std::size_t>(InputKey::MouseLeft) + i;
        const bool isLeft = i == 0; // The sample's mouse fields decide the left button
        const bool pressed = isLeft ? windowInput.mousePressed : windowInput.keysPressed[key];
        const bool down = isLeft ? windowInput.mouseDown : windowInput.keysDown[key];
        const bool released = isLeft ? windowInput.mouseReleased : windowInput.keysReleased[key];
        bool routedPressed = false, routedDown = false, routedReleased = false;
        if (pointerHere && pressed) {
            _buttons[i] = true;
            routedPressed = true;
        }
        if (_buttons[i]) {
            routedDown = down;
            if (released || !down) {
                routedReleased = true;
                _buttons[i] = false;
            }
        }
        routed.keysPressed[key] = routedPressed;
        routed.keysDown[key] = routedDown;
        routed.keysReleased[key] = routedReleased;
        if (isLeft) {
            routed.mousePressed = routedPressed;
            routed.mouseDown = routedDown;
            routed.mouseReleased = routedReleased;
        }
    }
    std::vector<int> touchIds;
    for (const auto& touch : windowInput.touches) {
        touchIds.push_back(touch.id);
        bool owned = std::ranges::find(_ownedTouches, touch.id) != _ownedTouches.end();
        if (!owned && std::ranges::find(_touchIds, touch.id) == _touchIds.end() && !pointerBlocked &&
            mapPoint(touch.position)) {
            _ownedTouches.push_back(touch.id);
            owned = true;
        }
        if (owned) routed.touches.push_back(touch);
    }
    std::erase_if(_ownedTouches, [&touchIds](int id) { return std::ranges::find(touchIds, id) == touchIds.end(); });
    _touchIds = std::move(touchIds);
    _lastMouse = position;
    auto result = _normalizer.Read(routed, mapPoint);
    // A blocked viewport must not expose the editor's mouse position to scene hit tests.
    if (!position || (pointerBlocked && !routed.mousePressed && !routed.mouseDown && !routed.mouseReleased))
        result.mousePosition.reset();
    return result;
}

void AccumulateViewportInput(DeviceInput& state, const DeviceInput& incoming) {
    if (incoming.cancelled) {
        state = DeviceInput{.cancelled = true};
        return;
    }
    if (state.pressPoints.size() + incoming.pressPoints.size() > WorkerMaxInputPoints)
        throw std::runtime_error("Scene input edge capacity exceeded");
    const float wheel = state.wheel + incoming.wheel;
    if (!std::isfinite(wheel)) throw std::runtime_error("Invalid accumulated input wheel");
    for (std::size_t i = 0; i < state.keysPressed.size(); ++i) {
        state.keysPressed[i] = state.keysPressed[i] || incoming.keysPressed[i];
        state.keysReleased[i] = state.keysReleased[i] || incoming.keysReleased[i];
    }
    state.keysDown = incoming.keysDown;
    state.mousePosition = incoming.mousePosition;
    state.mouseDown = incoming.mouseDown;
    state.mousePressed = state.mousePressed || incoming.mousePressed;
    state.mouseReleased = state.mouseReleased || incoming.mouseReleased;
    if (incoming.mousePress) state.mousePress = incoming.mousePress;
    if (incoming.mouseHover) state.mouseHover = incoming.mouseHover;
    state.wheel = wheel;
    state.heldPoints = incoming.heldPoints;
    state.touches = incoming.touches;
    state.pressPoints.insert(state.pressPoints.end(), incoming.pressPoints.begin(), incoming.pressPoints.end());
}
DeviceInput ConsumeViewportInput(DeviceInput& state) {
    // Deliver cancellation first; newer edges remain queued for the next worker frame.
    if (state.cancelled) {
        state.cancelled = false;
        return DeviceInput{.cancelled = true};
    }
    auto result = state;
    state.keysPressed.fill(false);
    state.keysReleased.fill(false);
    state.mousePressed = false;
    state.mouseReleased = false;
    state.mousePress.reset();
    state.mouseHover.reset();
    state.wheel = 0.0f;
    state.pressPoints.clear();
    return result;
}
