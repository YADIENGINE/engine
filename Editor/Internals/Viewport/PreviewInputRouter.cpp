#include "Editor/Internals/Viewport/PreviewInputRouter.h"
#include <algorithm>
#include <stdexcept>

PreviewInputRouter::PreviewInputRouter() = default;
DeviceInput PreviewInputRouter::Clear() {
    DeviceInput input;
    input.cancelled = true;
    _dragging = false;
    _suppressUntilNeutral = true;
    _lastMouse.reset();
    return input;
}
DeviceInput PreviewInputRouter::Route(const DeviceInput& windowInput, Float2 mouse, const PreviewViewport& viewport,
                                      bool windowFocused, bool viewportFocused, bool keyboardBlocked,
                                      bool pointerBlocked) {
    if (windowInput.cancelled || !windowFocused || viewport.size.x <= 0 || viewport.size.y <= 0 ||
        viewport.canvas.x <= 0 || viewport.canvas.y <= 0)
        return Clear();
    if (_suppressUntilNeutral) {
        const bool keyboardActive = std::ranges::any_of(windowInput.keysDown, [](bool down) { return down; }) ||
                                    std::ranges::any_of(windowInput.keysPressed, [](bool pressed) { return pressed; });
        _suppressUntilNeutral = keyboardActive || !windowInput.touches.empty() || windowInput.mouseDown ||
                                windowInput.mousePressed || windowInput.mouseReleased;
        return DeviceInput{.cancelled = true};
    }
    DeviceInput result;
    if (viewportFocused && !keyboardBlocked) {
        result.keysDown = windowInput.keysDown;
        result.keysPressed = windowInput.keysPressed;
        result.shiftHeld = windowInput.shiftHeld;
    }
    const auto position = MapPreviewPoint(viewport, mouse);
    if (position && !pointerBlocked) {
        result.mouse = position;
        if (position != _lastMouse) result.hover = position;
        result.wheel = windowInput.wheel;
        if (windowInput.mousePressed) {
            _dragging = true;
            result.mousePressed = true;
            result.pointer = position;
            result.taps.push_back(*position);
        }
    }
    if (_dragging) {
        result.mouse = position;
        result.mouseDown = windowInput.mouseDown;
        if (windowInput.mouseReleased || !windowInput.mouseDown) {
            result.mouseReleased = true;
            _dragging = false;
        }
        if (result.mouseDown && position) result.held.push_back(*position);
    }
    _lastMouse = position;
    return result;
}

void AccumulatePreviewInput(DeviceInput& state, const DeviceInput& incoming) {
    if (incoming.cancelled) {
        state = DeviceInput{.cancelled = true};
        return;
    }
    for (std::size_t i = 0; i < state.keysPressed.size(); ++i)
        state.keysPressed[i] = state.keysPressed[i] || incoming.keysPressed[i];
    state.keysDown = incoming.keysDown;
    state.shiftHeld = incoming.shiftHeld;
    state.mouse = incoming.mouse;
    state.mouseDown = incoming.mouseDown;
    state.mousePressed = state.mousePressed || incoming.mousePressed;
    state.mouseReleased = state.mouseReleased || incoming.mouseReleased;
    if (incoming.pointer) state.pointer = incoming.pointer;
    if (incoming.hover) state.hover = incoming.hover;
    state.wheel += incoming.wheel;
    state.held = incoming.held;
    if (state.taps.size() + incoming.taps.size() > 32) throw std::runtime_error("Preview input edge capacity exceeded");
    state.taps.insert(state.taps.end(), incoming.taps.begin(), incoming.taps.end());
}
DeviceInput ConsumePreviewInput(DeviceInput& state) {
    // Deliver cancellation first; newer edges remain queued for the next worker frame.
    if (state.cancelled) {
        state.cancelled = false;
        return DeviceInput{.cancelled = true};
    }
    auto result = state;
    state.keysPressed.fill(false);
    state.mousePressed = false;
    state.mouseReleased = false;
    state.pointer.reset();
    state.hover.reset();
    state.wheel = 0.0f;
    state.taps.clear();
    return result;
}
