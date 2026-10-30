#include "Input/Internals/PrimaryContact.h"
#include <algorithm>
#include <cstdlib>

bool InputDragThresholdReached(Vec2 point, Vec2 origin, int threshold) {
    return std::abs(point.x - origin.x) >= threshold || std::abs(point.y - origin.y) >= threshold;
}
void InputPrimaryContact::Begin(Vec2 point, InputContactFrame& result) {
    _origin = point;
    _last = point;
    result.pressed = point;
    result.position = point;
    result.origin = point;
}
void InputPrimaryContact::Track(Vec2 point, InputContactFrame& result) {
    _last = point;
    result.position = point;
    result.origin = _origin;
}
void InputPrimaryContact::Finish(InputContactFrame& result) {
    result.released = _last;
    result.origin = _origin;
    result.position.reset();
    _origin.reset();
    _last.reset();
}
void InputPrimaryContact::Cancel() {
    _cancelled = true;
    _touch.reset();
    _origin.reset();
    _last.reset();
    _mouseActive = false;
    _suppressMouse = true;
}
InputContactFrame InputPrimaryContact::Update(const DeviceInput& input) {
    InputContactFrame result;
    if (input.cancelled) Cancel();
    const bool quiet = input.touches.empty() && !input.mouseDown && !input.mousePressed && !input.mouseReleased;
    if (_cancelled) {
        result.cancelled = true;
        if (quiet) {
            _cancelled = false;
            _suppressMouse = false;
        }
        return result;
    }
    if (_touch) {
        result.touch = true;
        const auto found = std::ranges::find_if(input.touches, [this](const InputTouch& t) { return t.id == *_touch; });
        if (found == input.touches.end()) {
            _touch.reset();
            Finish(result);
        } else if (found->position) {
            Track(*found->position, result);
        } else {
            Cancel();
            result.cancelled = true;
        }
        return result;
    }
    if (_suppressMouse) {
        if (quiet) _suppressMouse = false;
        return result;
    }
    if (!_mouseActive && !input.touches.empty()) {
        const InputTouch& touch = input.touches.front();
        _touch = touch.id;
        _suppressMouse = true;
        result.touch = true;
        if (touch.position) {
            Begin(*touch.position, result);
        } else {
            Cancel();
            result.cancelled = true;
        }
        return result;
    }
    if (!_mouseActive && input.mousePressed) {
        const auto point = input.mousePress ? input.mousePress : input.mousePosition;
        if (!point) {
            Cancel();
            result.cancelled = true;
            return result;
        }
        _mouseActive = true;
        Begin(*point, result);
    }
    if (_mouseActive) {
        if (!input.mousePosition || (!input.mouseDown && !input.mouseReleased)) {
            Cancel();
            return InputContactFrame{.cancelled = true};
        }
        Track(*input.mousePosition, result);
        if (input.mouseReleased) {
            _mouseActive = false;
            Finish(result);
            // A touch that arrived during mouse ownership must not inherit that contact.
            if (!input.touches.empty()) _suppressMouse = true;
        }
        return result;
    }
    result.idle = !input.mouseDown && !input.mouseReleased;
    if (result.idle) result.hover = input.mouseHover;
    return result;
}
