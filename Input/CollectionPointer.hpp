#pragma once
#include <memory>
#include <optional>
#include "Utilities/Rect.h"

struct DeviceInput;
struct CollectionPointerResult {
    std::optional<Vec2> tap;
    std::optional<Vec2> hover;
    int rows = 0;
    bool cancelled = false;
};

// Release taps and continuous touch row scrolling, with latched drag cancellation.
class CollectionPointer {
public:
    CollectionPointer(const Rect& gridArea, int threshold);
    ~CollectionPointer();
    [[nodiscard]] CollectionPointerResult Update(const DeviceInput& input);
    // Cancel the current contact and ignore its release, including synthetic mouse events.
    void Cancel();

private:
    struct State;
    std::unique_ptr<State> _state;

public:
    CollectionPointer(const CollectionPointer&) = delete;
    CollectionPointer& operator=(const CollectionPointer&) = delete;
};
