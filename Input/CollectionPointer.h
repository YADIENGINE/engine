#pragma once
#include <optional>
#include "Utilities/Rect.h"

struct DeviceInput;
struct CollectionPointerResult {
    std::optional<Vec2> tap;
    std::optional<Vec2> hover;
    int rows = 0;
};

// Release taps and continuous touch row scrolling, with latched drag cancellation.
class CollectionPointer {
public:
    CollectionPointer(const Rect& gridArea, int threshold);
    [[nodiscard]] CollectionPointerResult Update(const DeviceInput& input);
    // Cancel the current contact and ignore its release, including synthetic mouse events.
    void Reset();

private:
    void Begin(std::optional<Vec2> point);
    int Move(std::optional<Vec2> point, bool scroll);
    [[nodiscard]] std::optional<Vec2> Finish();
    Rect _area;
    int _threshold;
    std::optional<int> _touch;
    std::optional<Vec2> _start;
    std::optional<Vec2> _last;
    std::optional<Vec2> _scrollOrigin;
    bool _dragged = false;
    bool _mouseActive = false;
    bool _suppressMouse = false;
    bool _cancelled = false;
};
