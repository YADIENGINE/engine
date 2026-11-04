#include "Camera/Cameras.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {
constexpr std::size_t MaxCameras = 4;

// Part 'index' of 'count' along one axis, 'gap' apart.
FloatRect Column(const FloatRect& area, std::size_t index, std::size_t count, float gap) {
    const float width = (area.width - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    return {area.x + (width + gap) * static_cast<float>(index), area.y, width, area.height};
}
FloatRect Row(const FloatRect& area, std::size_t index, std::size_t count, float gap) {
    const float height = (area.height - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    return {area.x, area.y + (height + gap) * static_cast<float>(index), area.width, height};
}
std::vector<FloatRect> Viewports(const FloatRect& canvas, std::size_t count, CameraSplit layout, float gap) {
    std::vector<FloatRect> parts;
    for (std::size_t i = 0; i < count; ++i) {
        if (count == 1) {
            parts.push_back(canvas);
        } else if (layout == CameraSplit::SideBySide || (layout == CameraSplit::Grid && count == 2)) {
            parts.push_back(Column(canvas, i, count, gap));
        } else if (layout == CameraSplit::Stacked) {
            parts.push_back(Row(canvas, i, count, gap));
        } else {
            const FloatRect row = Row(canvas, i / 2, 2, gap);
            const std::size_t inRow = i / 2 == 0 || count == 4 ? 2 : 1;
            parts.push_back(Column(row, i % 2, inRow, gap));
        }
    }
    return parts;
}
} // namespace

Cameras::Cameras(const FloatRect& canvas, const CameraSettings& settings)
    : _canvas(canvas) {
    _cameras.emplace_back(canvas, settings);
}
void Cameras::Split(std::size_t count, CameraSplit layout, float gap) {
    if (count < 1 || count > MaxCameras) throw std::invalid_argument("Cameras::Split takes 1 to 4 cameras");
    if (!std::isfinite(gap) || gap < 0.0f) throw std::invalid_argument("Cameras::Split needs a gap of 0 or more");
    _layout = layout;
    _gap = gap;
    const Camera2D main = _cameras.front(); // A copy: resizing may move the vector's elements
    _cameras.resize(count, main);
    Layout();
}
void Cameras::SetCanvas(const FloatRect& canvas) {
    _canvas = canvas;
    Layout();
}
void Cameras::Update(float seconds) {
    for (auto& camera : _cameras)
        camera.Update(seconds);
}
void Cameras::Snap() {
    for (auto& camera : _cameras)
        camera.Snap();
}
const Camera2D* Cameras::At(Float2 canvasPoint) const {
    for (const auto& camera : _cameras)
        if (camera.Viewport().Contains(canvasPoint)) return &camera;
    return nullptr;
}
void Cameras::Layout() {
    const auto parts = Viewports(_canvas, _cameras.size(), _layout, _gap);
    for (std::size_t i = 0; i < _cameras.size(); ++i)
        _cameras[i].SetViewport(parts[i]);
}
