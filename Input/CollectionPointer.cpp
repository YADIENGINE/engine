#include "Input/CollectionPointer.h"
#include <algorithm>
#include <cstdlib>
#include "Input/DeviceInput.h"

CollectionPointer::CollectionPointer(const Rect& gridArea, int threshold)
    : _area(gridArea)
    , _threshold(std::max(1, threshold)) {}

void CollectionPointer::Begin(std::optional<Vec2> point) {
    _start = point;
    _last = point;
    _scrollOrigin = point && _area.Contains(*point) ? point : std::nullopt;
    _dragged = false;
}
int CollectionPointer::Move(std::optional<Vec2> point, bool scroll) {
    _last = point;
    if (!point) {
        _start.reset();
        _scrollOrigin.reset();
        return 0;
    }
    if (_start && (std::abs(point->x - _start->x) >= _threshold || std::abs(point->y - _start->y) >= _threshold))
        _dragged = true;
    if (!scroll || !_scrollOrigin) return 0;
    const int dx = point->x - _scrollOrigin->x;
    const int dy = point->y - _scrollOrigin->y;
    if (std::abs(dy) < _threshold || std::abs(dy) <= std::abs(dx)) return 0;
    _scrollOrigin = point;
    return dy < 0 ? 1 : -1;
}
std::optional<Vec2> CollectionPointer::Finish() {
    const auto tap = _start && !_dragged ? _last : std::nullopt;
    _start.reset();
    _last.reset();
    _scrollOrigin.reset();
    return tap;
}
void CollectionPointer::Reset() {
    _cancelled = true;
    _start.reset();
    _scrollOrigin.reset();
    _suppressMouse = true;
}
CollectionPointerResult CollectionPointer::Update(const DeviceInput& input) {
    CollectionPointerResult result;
    if (input.cancelled) Reset();
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
        if (found != input.touches.end())
            result.rows = Move(found->position, true);
        else {
            _touch.reset();
            result.tap = Finish();
        }
        return result;
    }
    if (_suppressMouse) {
        if (quiet) _suppressMouse = false;
        return result;
    }
    if (!input.touches.empty()) {
        const auto& touch = input.touches.front();
        _touch = touch.id;
        _mouseActive = false;
        _suppressMouse = true;
        Begin(touch.position);
        return result;
    }
    if (input.mousePressed) {
        _mouseActive = true;
        Begin(input.mouse);
    }
    if (_mouseActive) {
        (void)Move(input.mouse, false);
        if (input.mouseReleased) {
            _mouseActive = false;
            result.tap = Finish();
        } else if (!input.mouseDown)
            Reset();
        return result;
    }
    if (!input.mouseReleased && !input.mouseDown) result.hover = input.hover;
    if (input.wheel != 0.0f && input.mouse && _area.Contains(*input.mouse)) {
        result.rows = input.wheel > 0.0f ? -1 : 1;
        result.hover.reset();
    }
    return result;
}
