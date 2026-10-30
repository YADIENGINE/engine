#include "Graphics/Render/UiRegion.h"
#include <raymath.h>
#include <rlgl.h>
#include "Graphics/Render/Internals/RenderTargetStack.h"

UiRegion::UiRegion(Rect source, Rect target)
    : _transform(source, target) {}
Vector2 UiRegion::Map(Vector2 point) const {
    const Float2 mapped = _transform.Map(Float2{point.x, point.y});
    return {mapped.x, mapped.y};
}
Vector2 UiRegion::Unmap(Vector2 point) const {
    const Float2 local = _transform.Unmap(Float2{point.x, point.y});
    return {local.x, local.y};
}
Vec2 UiRegion::Unmap(Vec2 point) const { return _transform.Unmap(point); }
Camera2D UiRegion::Camera() const {
    const Float2 offset = _transform.Offset();
    return {{offset.x, offset.y}, {}, 0.0f, _transform.Scale()};
}

void BeginUiClip(Rectangle rect) {
    const Matrix transform = rlGetMatrixModelview();
    const Vector3 from = Vector3Transform({rect.x, rect.y, 0}, transform);
    const Vector3 to = Vector3Transform({rect.x + rect.width, rect.y + rect.height, 0}, transform);
    BeginTargetScissor({from.x, from.y, to.x - from.x, to.y - from.y});
}
