#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Viewport/SceneViewport.h"
#include "Utilities/Geometry/FloatRect.h"

struct ViewportImage {
    std::uint64_t texture = 0;
    int width = 0;
    int height = 0;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    double transferMs = 0;
};

struct EditorViewportInteraction {
    SceneViewport viewport;
    bool focused = false;
    bool hovered = false;
    bool contentActive = false;
    bool toggleMaximize = false;
    bool marking = false;          // A press goes to marking, not the product
    bool markDragging = false;
    bool pauseForMarking = false;  // Explicit pause, never a toggle; handled by the host.
    std::optional<FloatRect> mark; // A finished mark, canvas units
};

class ViewportPanel {
public:
    [[nodiscard]] EditorViewportInteraction Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                                 const ViewportImage& image, const std::vector<std::string>& recent,
                                                 bool metrics, bool maximized, bool allowMarkGesture = true);
    void ReleaseFocus() { _focused = false; }

private:
    [[nodiscard]] bool Toolbar(bool maximized);
    bool _focused = false;
    int _mode = 1;
    std::size_t _dataView = 0; // The inspection table shown
    float _zoom = 1.0f;        // Multiplies the mode's own scale
    float _shownScale = 0.0f;  // Screen pixels per canvas pixel in the last drawn frame; 0 while there is none
    Float2 _pan;
    std::string _scene;
    std::optional<Float2> _markStart; // Screen point where the current mark drag began
};
