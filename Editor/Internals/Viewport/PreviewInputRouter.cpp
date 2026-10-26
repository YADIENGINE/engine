#include "Editor/Internals/Viewport/PreviewInputRouter.h"
#include <stdexcept>

PreviewInputRouter::PreviewInputRouter() = default;
DeviceInput PreviewInputRouter::Clear() {
    DeviceInput input;
    input.mouseReleased = _dragging;
    _dragging = false;
    _lastMouse.reset();
    return input;
}
DeviceInput PreviewInputRouter::Route(const DeviceInput& windowInput, Float2 mouse, const PreviewViewport& viewport,
                                      bool windowFocused, bool viewportFocused, bool keyboardBlocked,
                                      bool pointerBlocked) {
    if (!windowFocused || viewport.size.x <= 0 || viewport.size.y <= 0 || viewport.canvas.x <= 0 ||
        viewport.canvas.y <= 0)
        return Clear();
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
