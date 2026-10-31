#pragma once
#include "Utilities/Affine2.h"
#include "Utilities/Float2.h"
#include "Utilities/Rect.h"

// Centered, aspect-preserving source-to-target mapping. Integer results round to the nearest pixel.
class UiTransform {
public:
    UiTransform(Rect source, Rect target);
    [[nodiscard]] Float2 Map(Float2 point) const;
    [[nodiscard]] Float2 Unmap(Float2 point) const;
    [[nodiscard]] Vec2 Map(Vec2 point) const;
    [[nodiscard]] Vec2 Unmap(Vec2 point) const;
    [[nodiscard]] float Scale() const { return _scale; }
    [[nodiscard]] Float2 Offset() const { return _offset; }
    // The same mapping for drawing: auto region = paint.Transform(transform.Affine());
    [[nodiscard]] Affine2 Affine() const { return Affine2::Translate(_offset) * Affine2::Scale(_scale); }

private:
    Vec2 _sourceCenter;
    Vec2 _targetCenter;
    float _scale;
    Float2 _offset;
};
