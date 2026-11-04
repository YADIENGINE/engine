#include "Camera/Debug/CameraGuides.h"
#include <algorithm>
#include <array>
#include "Camera/Camera2D.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"

namespace {
constexpr float MarkSize = 6.0f;
constexpr float SmallestTarget = 4.0f;

// A world rectangle as the camera shows it: four corners, turned with the camera.
void WorldRect(Painter& paint, const CameraView& view, const FloatRect& rect, const LineStyle& line) {
    const std::array<Float2, 4> corners{view.ToScreen(rect.Position()), view.ToScreen({rect.Right(), rect.y}),
                                        view.ToScreen({rect.Right(), rect.Bottom()}),
                                        view.ToScreen({rect.x, rect.Bottom()})};
    paint.Polyline(corners, line, true);
}
} // namespace

void DrawCameraGuides(Painter& paint, const Camera2D& camera, const CameraGuidesStyle& style) {
    const CameraView view = camera.View();
    const auto& settings = camera.Settings();
    auto clip = paint.Clip(camera.Viewport());
    paint.Rect(camera.Viewport().Inset(style.width * 0.5f), Outline(style.width, style.viewport));
    if (settings.limits) WorldRect(paint, view, *settings.limits, {.width = style.width, .color = style.limits});
    const Float2 visible = camera.Viewport().Size() / camera.Pose().zoom;
    const Float2 zone{visible.x * settings.follow.deadZone.x, visible.y * settings.follow.deadZone.y};
    if (zone.x > 0.0f || zone.y > 0.0f) {
        const LineStyle line{.width = style.width, .color = style.deadZone};
        WorldRect(paint, view, FloatRect::FromCenter(camera.Goal(), zone), line);
    }
    for (const auto& target : camera.Targets()) {
        const float radius = std::max(target.radius * camera.Pose().zoom, SmallestTarget);
        paint.Circle(view.ToScreen(target.position), radius, Outline(style.width, style.targets));
    }
    if (camera.Targets().empty()) return;
    const Float2 focus = view.ToScreen(camera.Focus());
    const LineStyle mark{.width = style.width, .color = style.focus};
    paint.Line(focus - Float2{MarkSize, MarkSize}, focus + Float2{MarkSize, MarkSize}, mark);
    paint.Line(focus - Float2{MarkSize, -MarkSize}, focus + Float2{MarkSize, -MarkSize}, mark);
    paint.Circle(view.ToScreen(camera.Goal()), MarkSize * 0.5f, Fill(style.focus));
}
