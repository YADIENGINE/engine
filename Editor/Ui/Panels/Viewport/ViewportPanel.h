#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Viewport/SceneViewport.h"
#include "Utilities/FloatRect.h"

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
    std::optional<FloatRect> mark; // A finished mark, canvas units
};

class ViewportPanel {
public:
    [[nodiscard]] EditorViewportInteraction Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                                 const ViewportImage& image, bool metrics, bool maximized);
    void ReleaseFocus() { _focused = false; }

private:
    [[nodiscard]] bool Toolbar(bool maximized);
    bool _focused = false;
    int _mode = 1;
    float _zoom = 1.0f;
    Float2 _pan;
    std::string _scene;
    std::optional<Float2> _markStart; // Screen point where the current mark drag began
};
