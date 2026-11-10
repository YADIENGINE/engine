#include "Editor/Internals/Pixelator/PixelatorView.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr std::array ZoomLadder{1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
}

void PixelatorView::Keep(Float2 area, Vec2 sprite) {
    if (_fitted && (area != _area || sprite.x != _sprite.x || sprite.y != _sprite.y)) Fit(area, sprite);
}

void PixelatorView::Centre(Float2 area, Vec2 sprite) {
    _area = area;
    _sprite = sprite;
    const auto zoom = static_cast<float>(_zoom);
    _origin = {std::round((area.x - static_cast<float>(sprite.x) * zoom) * 0.5f),
               std::round((area.y - static_cast<float>(sprite.y) * zoom) * 0.5f)};
}

void PixelatorView::Fit(Float2 area, Vec2 sprite) {
    int zoom = MaxZoom;
    if (sprite.x > 0 && sprite.y > 0)
        zoom = static_cast<int>(
            std::floor(std::min(area.x / static_cast<float>(sprite.x), area.y / static_cast<float>(sprite.y))));
    _zoom = std::clamp(zoom, MinZoom, MaxZoom);
    Centre(area, sprite);
    _fitted = true;
}

void PixelatorView::Actual(Float2 area, Vec2 sprite) {
    _zoom = 1;
    Centre(area, sprite);
    _fitted = false;
}

int PixelatorView::StepZoom(int zoom, int steps) {
    if (steps > 0) {
        const auto next = std::ranges::upper_bound(ZoomLadder, zoom);
        const auto last = static_cast<std::ptrdiff_t>(ZoomLadder.size()) - 1;
        const auto index = std::min<std::ptrdiff_t>(next - ZoomLadder.begin() + steps - 1, last);
        return next == ZoomLadder.end() ? MaxZoom : ZoomLadder[static_cast<std::size_t>(index)];
    }
    if (steps < 0) {
        const auto below = std::ranges::lower_bound(ZoomLadder, zoom); // First rung >= zoom
        const auto index = (below - ZoomLadder.begin()) + steps;
        return index < 0 ? MinZoom : ZoomLadder[static_cast<std::size_t>(index)];
    }
    return std::clamp(zoom, MinZoom, MaxZoom);
}

void PixelatorView::ZoomAt(Float2 point, int steps) { SetZoom(StepZoom(_zoom, steps), point); }

void PixelatorView::SetZoom(int zoom, Float2 point) {
    zoom = std::clamp(zoom, MinZoom, MaxZoom);
    if (zoom == _zoom) return;
    const Float2 under = (point - _origin) * (1.0f / static_cast<float>(_zoom)); // In sprite pixels
    _zoom = zoom;
    _origin = {std::round(point.x - under.x * static_cast<float>(zoom)),
               std::round(point.y - under.y * static_cast<float>(zoom))};
    _fitted = false;
}

void PixelatorView::Pan(Float2 delta) {
    if (delta == Float2{}) return;
    _origin = {std::round(_origin.x + delta.x), std::round(_origin.y + delta.y)};
    _fitted = false;
}

std::optional<Vec2> PixelatorView::PixelAt(Float2 point, Vec2 sprite) const {
    const auto zoom = static_cast<float>(_zoom);
    const int x = static_cast<int>(std::floor((point.x - _origin.x) / zoom));
    const int y = static_cast<int>(std::floor((point.y - _origin.y) / zoom));
    if (x < 0 || y < 0 || x >= sprite.x || y >= sprite.y) return std::nullopt;
    return Vec2{x, y};
}

Float2 PixelatorView::ScreenOf(Float2 pixel) const { return _origin + pixel * static_cast<float>(_zoom); }
