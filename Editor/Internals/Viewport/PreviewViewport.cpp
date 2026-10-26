#include "Editor/Internals/Viewport/PreviewViewport.h"
#include <algorithm>
#include <cmath>

PreviewViewport FitPreviewViewport(Float2 position, Float2 available, Vec2 canvas) {
    if (canvas.x <= 0 || canvas.y <= 0 || available.x <= 0.0f || available.y <= 0.0f) return {position, {}, canvas};
    const float scale =
        std::min(available.x / static_cast<float>(canvas.x), available.y / static_cast<float>(canvas.y));
    const Float2 size{static_cast<float>(canvas.x) * scale, static_cast<float>(canvas.y) * scale};
    return {{position.x + (available.x - size.x) * 0.5f, position.y + (available.y - size.y) * 0.5f}, size, canvas};
}
std::optional<Vec2> MapPreviewPoint(const PreviewViewport& viewport, Float2 point) {
    if (viewport.clipSize.x > 0 && viewport.clipSize.y > 0 &&
        (point.x < viewport.clipPosition.x || point.y < viewport.clipPosition.y ||
         point.x >= viewport.clipPosition.x + viewport.clipSize.x ||
         point.y >= viewport.clipPosition.y + viewport.clipSize.y))
        return std::nullopt;
    if (viewport.size.x <= 0.0f || viewport.size.y <= 0.0f || viewport.canvas.x <= 0 || viewport.canvas.y <= 0 ||
        !std::isfinite(point.x) || !std::isfinite(point.y) || point.x < viewport.position.x ||
        point.y < viewport.position.y || point.x >= viewport.position.x + viewport.size.x ||
        point.y >= viewport.position.y + viewport.size.y)
        return std::nullopt;
    return Vec2{
        std::min(viewport.canvas.x - 1, static_cast<int>((point.x - viewport.position.x) *
                                                         static_cast<float>(viewport.canvas.x) / viewport.size.x)),
        std::min(viewport.canvas.y - 1, static_cast<int>((point.y - viewport.position.y) *
                                                         static_cast<float>(viewport.canvas.y) / viewport.size.y))};
}
