#pragma once
#include <functional>
#include <vector>
#include "Input/Internals/InputSample.h"

// Converts a backend sample to the public canvas contract and tracks contact beginnings.
class InputNormalizer {
public:
    [[nodiscard]] DeviceInput Read(const InputSample& sample,
                                   const std::function<std::optional<Vec2>(Float2)>& mapPoint);
    // Keep sampling during transitions; the neutral frame is also cancelled.
    void Cancel();

private:
    std::vector<int> _touchIds;
    bool _suppressMouse = false;
    bool _suppressUntilNeutral = false;
};
