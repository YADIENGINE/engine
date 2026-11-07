#pragma once
#include <algorithm>
#include <array>
#include "Editor/Internals/Viewport/SceneViewport.h"
#include "Input/Internals/InputNormalizer.h"

class ViewportInputRouter {
public:
    ViewportInputRouter();
    [[nodiscard]] bool OwnsPointer() const {
        return std::ranges::any_of(_buttons, [](bool owned) { return owned; }) || !_ownedTouches.empty();
    }
    [[nodiscard]] DeviceInput Route(const InputSample& windowInput, Float2 mouse, const SceneViewport& viewport,
                                    bool windowFocused, bool viewportFocused, bool keyboardBlocked,
                                    bool pointerBlocked);
    // Cancels owned input; held devices must become neutral before routing resumes.
    [[nodiscard]] DeviceInput Clear();

private:
    InputNormalizer _normalizer;
    std::vector<int> _touchIds;
    std::vector<int> _ownedTouches;
    // Mouse buttons (MouseLeft, MouseRight, MouseMiddle) pressed in the viewport, routed until released.
    std::array<bool, 3> _buttons{};
    bool _suppressUntilNeutral = false;
    std::optional<Vec2> _lastMouse;
};

// Preserve edges and cancellation order when several editor snapshots arrive in one worker frame.
void AccumulateViewportInput(DeviceInput& state, const DeviceInput& incoming);
[[nodiscard]] DeviceInput ConsumeViewportInput(DeviceInput& state);
