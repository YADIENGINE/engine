#include "Graphics/Light/Debug/LightGuides.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Light/LightOccluders.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Paint/PaintStyle.h"

namespace {
constexpr float MarkSize = 6.0f;
constexpr float SmallestZoom = 1e-3f;
} // namespace

void DrawLightGuides(Painter& paint, const LightFrame& frame, const CameraView& view, const LightGuidesStyle& style) {
    std::optional<PaintScope> clip;
    if (!view.viewport.Empty()) clip.emplace(paint.Clip(view.viewport));
    auto camera = paint.Camera(CameraView{view.offset, view.target, view.rotation, view.zoom, {}});
    // Lines keep their width and the mark its size on screen, however the camera zooms.
    const float unit = 1.0f / std::max(view.zoom, SmallestZoom);
    const float width = style.width * unit;
    const float mark = MarkSize * unit;
    const LineStyle reach{.width = width, .color = style.reach};
    const LineStyle stops{.width = width, .color = style.occluder};
    for (const LightOccluders* occluders : frame.occluders) {
        if (!occluders) continue;
        for (const VisibilityEdge& edge : occluders->Edges())
            paint.Line(edge.a, edge.b, stops);
    }
    const auto drawLight = [&](const LightSource& light) {
        if (!(light.radius > 0.0f)) return;
        const Float2 at = light.at;
        paint.Line(at - Float2{mark, mark}, at + Float2{mark, mark}, reach);
        paint.Line(at - Float2{mark, -mark}, at + Float2{mark, -mark}, reach);
        const LightCone& cone = light.cone;
        const bool whole = cone.halfAngle >= Pi;
        if (light.core > 0.0f) {
            const float core = light.radius * std::min(light.core, 1.0f);
            if (whole) {
                paint.Circle(at, core, Outline(width, style.core));
            } else {
                paint.Arc(at, core, cone.direction - cone.halfAngle, cone.halfAngle * 2.0f,
                          {.width = width, .color = style.core});
            }
        }
        if (whole) {
            paint.Circle(at, light.radius, Outline(width, style.reach));
            return;
        }
        paint.Arc(at, light.radius, cone.direction - cone.halfAngle, cone.halfAngle * 2.0f, reach);
        paint.Line(at, at + Float2::FromAngle(cone.direction - cone.halfAngle, light.radius), reach);
        paint.Line(at, at + Float2::FromAngle(cone.direction + cone.halfAngle, light.radius), reach);
    };
    for (const LightSource& light : frame.lights)
        drawLight(light);
    for (const LightSource& light : frame.flashes.lights)
        drawLight(light);
}
