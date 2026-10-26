#include "Input/DragPointer.h"
#include <algorithm>
#include <cstdlib>
#include "Input/DeviceInput.h"

DragPointer::DragPointer(int threshold)
    : _threshold(std::max(1, threshold)) {}

void DragPointer::Begin(Vec2 point, DragPointerResult& result) {
    _origin = point;
    _last = point;
    _dragged = false;
    result.pressed = point;
    result.position = point;
    result.origin = point;
}

void DragPointer::Track(Vec2 point, DragPointerResult& result) {
    _last = point;
    if (_origin && (std::abs(point.x - _origin->x) >= _threshold || std::abs(point.y - _origin->y) >= _threshold))
        _dragged = true;
    result.position = point;
    result.origin = _origin;
    result.dragging = _dragged;
}

void DragPointer::Finish(DragPointerResult& result) {
    result.released = _last;
    result.origin = _origin;
    result.dragging = _dragged;
    result.tap = _last.has_value() && !_dragged;
    _origin.reset();
    _last.reset();
    _dragged = false;
}

void DragPointer::Reset() {
    _cancelled = true;
    _origin.reset();
    _last.reset();
    _dragged = false;
    _suppressMouse = true;
}

DragPointerResult DragPointer::Update(const DeviceInput& input) {
    DragPointerResult result;
    const bool quiet = input.touches.empty() && !input.mouseDown && !input.mousePressed && !input.mouseReleased;
    if (_cancelled) {
        if (quiet) {
            _cancelled = false;
            _touch.reset();
            _mouseActive = false;
            _suppressMouse = false;
        }
        return result;
    }
    if (_touch) {
        const auto found = std::ranges::find_if(input.touches, [this](const InputTouch& t) { return t.id == *_touch; });
        if (found == input.touches.end()) {
            _touch.reset();
            Finish(result);
        } else if (found->position) {
            Track(*found->position, result);
        } else {
            // Canvas dışına çıkan parmak: son konumda kalır.
            result.position = _last;
            result.origin = _origin;
            result.dragging = _dragged;
        }
        return result;
    }
    if (_suppressMouse) {
        if (quiet) _suppressMouse = false;
        return result;
    }
    if (!input.touches.empty()) {
        const InputTouch& touch = input.touches.front();
        _touch = touch.id;
        _mouseActive = false;
        _suppressMouse = true;
        if (touch.position)
            Begin(*touch.position, result);
        else
            _origin.reset();
        return result;
    }
    if (input.mousePressed && input.mouse) {
        _mouseActive = true;
        Begin(*input.mouse, result);
        if (!input.mouseReleased) return result;
    }
    if (_mouseActive) {
        if (input.mouse) Track(*input.mouse, result);
        if (input.mouseReleased) {
            _mouseActive = false;
            Finish(result);
            result.position.reset();
        } else if (!input.mouseDown) {
            Reset();
            return {};
        }
        return result;
    }
    if (!input.mouseDown) result.hover = input.hover;
    return result;
}
