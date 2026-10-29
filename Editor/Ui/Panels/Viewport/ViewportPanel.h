#pragma once
#include <cstdint>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Viewport/PreviewViewport.h"

struct EditorPreviewImage {
    std::uint64_t texture = 0;
    int width = 0;
    int height = 0;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    double transferMs = 0;
};

struct EditorViewportInteraction {
    PreviewViewport viewport;
    bool focused = false;
    bool hovered = false;
    bool contentActive = false;
    bool toggleMaximize = false;
};

class ViewportPanel {
public:
    [[nodiscard]] EditorViewportInteraction Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                                 const EditorPreviewImage& image, bool metrics, bool maximized);
    void ReleaseFocus() { _focused = false; }

private:
    [[nodiscard]] bool Toolbar(bool maximized);
    bool _focused = false;
    int _mode = 1;
    float _zoom = 1.0f;
    Float2 _pan;
    std::string _preview;
};
