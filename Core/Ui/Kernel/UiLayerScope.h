#pragma once
#include <optional>
#include <utility>
#include "Ui/Kernel/UiTypes.h"

class UiKernel;

struct UiLayerSpec {
    std::optional<float> seconds{}; // Time to open and to close; zero snaps. Empty: 0.15, or the look's
    bool modal = true;              // Input does not reach the layers below
    UiDirection direction = UiDirection::Column;
    float gap = 0.0f;
    UiInsets padding{};
    UiAlign alignX = UiAlign::Start;
    UiAlign alignY = UiAlign::Start;
};

// An open layer. It is true while the layer is on screen; elements added meanwhile belong to it.
class [[nodiscard]] UiLayerScope {
public:
    UiLayerScope(UiKernel& kernel, UiId id, bool open, const UiLayerSpec& spec);
    UiLayerScope(UiLayerScope&& other) noexcept
        : _kernel(std::exchange(other._kernel, nullptr))
        , _root(other._root)
        , _savedCursor(other._savedCursor)
        , _savedLayer(other._savedLayer)
        , _progress(other._progress)
        , _closing(other._closing) {}
    ~UiLayerScope();

    [[nodiscard]] explicit operator bool() const { return _kernel != nullptr; }
    // 0: fully closed; 1: fully open. Unshaped: pass it through a Curve for the look you want.
    [[nodiscard]] float Open() const { return _progress; }
    [[nodiscard]] bool Closing() const { return _closing; }
    // Called while drawing, under the layer's elements: a dimmed backdrop, for example.
    void Draw(UiDrawFunction draw);
    void SetVisual(const UiVisual& visual);

private:
    UiKernel* _kernel; // Null when the layer is closed, or once moved from
    int _root = -1;
    int _savedCursor = -1;
    int _savedLayer = 0;
    float _progress = 0.0f;
    bool _closing = false;

public:
    UiLayerScope(const UiLayerScope&) = delete;
    UiLayerScope& operator=(const UiLayerScope&) = delete;
    UiLayerScope& operator=(UiLayerScope&&) = delete;
};
