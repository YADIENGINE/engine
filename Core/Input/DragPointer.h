#pragma once
#include <memory>
#include <optional>
#include "Utilities/Vec2.h"

struct DeviceInput;
// State of one primary contact (touch or left mouse button) during a frame.
struct DragPointerResult {
    std::optional<Vec2> pressed;  // Contact began this frame (starting position)
    std::optional<Vec2> position; // Contact is ongoing: current position
    std::optional<Vec2> origin;   // Contact is ongoing or ended this frame: starting position
    std::optional<Vec2> released; // Contact ended this frame: final position
    std::optional<Vec2> hover;    // Mouse hover when no contact is held
    bool dragging = false;        // Contact moved beyond the threshold (also true in the ending frame)
    bool tap = false;             // Contact ended without crossing the threshold
    bool cancelled = false;       // Contact was cancelled; clear activation/drop and local drag visuals.
};

// Drag and drop: tracks the start, position and end of one primary contact. Ignores synthetic mouse events from
// touch; other fingers do not disrupt the primary contact. A contact leaving the canvas is cancelled.
class DragPointer {
public:
    explicit DragPointer(int threshold);
    ~DragPointer();
    [[nodiscard]] DragPointerResult Update(const DeviceInput& input);
    // Cancels the ongoing contact; its release (and synthetic mouse events) is ignored.
    void Cancel();

private:
    struct State;
    std::unique_ptr<State> _state;

public:
    DragPointer(const DragPointer&) = delete;
    DragPointer& operator=(const DragPointer&) = delete;
};
