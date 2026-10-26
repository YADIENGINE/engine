#pragma once
#include <raylib.h>
#include "Utilities/UiTransform.h"

// Draw in a widget's design coordinates; input uses the inverse. Call without another active 2D camera.
class UiRegion {
public:
    UiRegion(Rect source, Rect target);
    [[nodiscard]] Vector2 Map(Vector2 point) const;
    [[nodiscard]] Vector2 Unmap(Vector2 point) const;
    [[nodiscard]] Vec2 Unmap(Vec2 point) const;
    template <class Drawing> void Draw(Drawing drawing) const {
        BeginMode2D(Camera());
        try {
            drawing();
        } catch (...) {
            EndMode2D();
            throw;
        }
        EndMode2D();
    }

private:
    [[nodiscard]] Camera2D Camera() const;
    UiTransform _transform;
};

// Axis-aligned clipping in the current design coordinates. Replaces rather than stacks scissor regions.
// The caller ends the scissor or reapplies its outer clip; rotation/negative scale are not supported.
void BeginUiClip(Rectangle rect);
