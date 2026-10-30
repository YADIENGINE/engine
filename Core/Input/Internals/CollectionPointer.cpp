#include "Input/CollectionPointer.hpp"
#include <algorithm>
#include <cstdlib>
#include "Input/Internals/PrimaryContact.h"

struct CollectionPointer::State {
    InputPrimaryContact contact;
    Rect area;
    int threshold = 1;
    std::optional<Vec2> scrollOrigin;
    bool dragged = false;
};

CollectionPointer::CollectionPointer(const Rect& gridArea, int threshold)
    : _state(std::make_unique<State>()) {
    _state->area = gridArea;
    _state->threshold = std::max(1, threshold);
}
CollectionPointer::~CollectionPointer() = default;
void CollectionPointer::Cancel() {
    _state->contact.Cancel();
    _state->scrollOrigin.reset();
    _state->dragged = false;
}
CollectionPointerResult CollectionPointer::Update(const DeviceInput& input) {
    const auto contact = _state->contact.Update(input);
    if (contact.cancelled) {
        _state->scrollOrigin.reset();
        _state->dragged = false;
        return CollectionPointerResult{.cancelled = true};
    }
    if (contact.pressed) {
        _state->dragged = false;
        _state->scrollOrigin = _state->area.Contains(*contact.pressed) ? contact.pressed : std::nullopt;
    }
    const auto point = contact.position ? contact.position : contact.released;
    if (point && contact.origin && InputDragThresholdReached(*point, *contact.origin, _state->threshold))
        _state->dragged = true;
    CollectionPointerResult result;
    if (contact.touch && contact.position && _state->scrollOrigin) {
        const int dx = contact.position->x - _state->scrollOrigin->x;
        const int dy = contact.position->y - _state->scrollOrigin->y;
        if (std::abs(dy) >= _state->threshold && std::abs(dy) > std::abs(dx)) {
            result.rows = dy < 0 ? 1 : -1;
            _state->scrollOrigin = contact.position;
        }
    }
    if (contact.released) {
        if (!_state->dragged) result.tap = contact.released;
        _state->scrollOrigin.reset();
        _state->dragged = false;
    }
    result.hover = contact.hover;
    if (contact.idle && input.wheel != 0.0f && input.mousePosition && _state->area.Contains(*input.mousePosition)) {
        result.rows = input.wheel > 0.0f ? -1 : 1;
        result.hover.reset();
    }
    return result;
}
