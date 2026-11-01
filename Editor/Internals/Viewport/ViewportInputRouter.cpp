#include "Editor/Internals/Viewport/ViewportInputRouter.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Editor/Internals/Protocol/WorkerProtocol.h"

ViewportInputRouter::ViewportInputRouter() = default;
DeviceInput ViewportInputRouter::Clear() {
    _dragging = false;
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
    }
    const auto position = mapPoint(mouse);
    routed.mousePosition = mouse;
    if (position && !pointerBlocked) {
        routed.mouseMoved = position != _lastMouse;
        routed.wheel = windowInput.wheel;
        if (windowInput.mousePressed) {
            _dragging = true;
            routed.mousePressed = true;
        }
    }
    if (_dragging) {
        routed.mouseDown = windowInput.mouseDown;
        if (windowInput.mouseReleased || !windowInput.mouseDown) {
            routed.mouseReleased = true;
            _dragging = false;
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
    for (std::size_t i = 0; i < state.keysPressed.size(); ++i)
        state.keysPressed[i] = state.keysPressed[i] || incoming.keysPressed[i];
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
    state.mousePressed = false;
    state.mouseReleased = false;
    state.mousePress.reset();
    state.mouseHover.reset();
    state.wheel = 0.0f;
    state.pressPoints.clear();
    return result;
}
