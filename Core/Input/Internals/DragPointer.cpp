#include "Input/DragPointer.hpp"
#include <algorithm>
#include "Input/Internals/PrimaryContact.h"

struct DragPointer::State {
    InputPrimaryContact contact;
    int threshold = 1;
    bool dragged = false;
};

DragPointer::DragPointer(int threshold)
    : _state(std::make_unique<State>()) {
    _state->threshold = std::max(1, threshold);
}
DragPointer::~DragPointer() = default;
void DragPointer::Cancel() {
    _state->contact.Cancel();
    _state->dragged = false;
}
DragPointerResult DragPointer::Update(const DeviceInput& input) {
    const auto contact = _state->contact.Update(input);
    if (contact.cancelled) {
        _state->dragged = false;
        return DragPointerResult{.cancelled = true};
    }
    if (contact.pressed) _state->dragged = false;
    const auto point = contact.position ? contact.position : contact.released;
    if (point && contact.origin && InputDragThresholdReached(*point, *contact.origin, _state->threshold))
        _state->dragged = true;
    DragPointerResult result{.pressed = contact.pressed,
                             .position = contact.position,
                             .origin = contact.origin,
                             .released = contact.released,
                             .hover = contact.hover,
                             .dragging = point.has_value() && _state->dragged,
                             .tap = contact.released.has_value() && !_state->dragged};
    if (contact.released) _state->dragged = false;
    return result;
}
