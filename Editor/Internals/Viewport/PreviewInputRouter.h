#pragma once
#include "Editor/Internals/Viewport/PreviewViewport.h"
#include "Input/DeviceInput.h"

class PreviewInputRouter {
public:
    PreviewInputRouter();
    [[nodiscard]] bool OwnsPointer() const { return _dragging; }
    [[nodiscard]] DeviceInput Route(const DeviceInput& windowInput, Float2 mouse, const PreviewViewport& viewport,
                                    bool windowFocused, bool viewportFocused, bool keyboardBlocked,
                                    bool pointerBlocked);
    // Delivers a release for an owned drag, then clears ownership.
    [[nodiscard]] DeviceInput Clear();

private:
    bool _dragging = false;
    std::optional<Vec2> _lastMouse;
};

// Preserve press/release edges when several editor snapshots arrive in one worker frame.
void AccumulatePreviewInput(DeviceInput& state, const DeviceInput& incoming);
[[nodiscard]] DeviceInput ConsumePreviewInput(DeviceInput& state);
