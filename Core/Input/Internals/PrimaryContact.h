#pragma once
#include <optional>
#include "Input/Input.h"

struct InputContactFrame {
    std::optional<Vec2> pressed;
    std::optional<Vec2> position;
    std::optional<Vec2> origin;
    std::optional<Vec2> released;
    std::optional<Vec2> hover;
    bool touch = false;
    bool idle = false;
    bool cancelled = false;
};

// Owns contact identity, bounds cancellation and synthetic mouse suppression for one gesture instance.
class InputPrimaryContact {
public:
    [[nodiscard]] InputContactFrame Update(const DeviceInput& input);
    void Cancel();

private:
    void Begin(Vec2 point, InputContactFrame& result);
    void Track(Vec2 point, InputContactFrame& result);
    void Finish(InputContactFrame& result);
    std::optional<int> _touch;
    std::optional<Vec2> _origin;
    std::optional<Vec2> _last;
    bool _mouseActive = false;
    bool _suppressMouse = false;
    bool _cancelled = false;
};

[[nodiscard]] bool InputDragThresholdReached(Vec2 point, Vec2 origin, int threshold);
