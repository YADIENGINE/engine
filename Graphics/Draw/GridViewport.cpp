#include "Graphics/Draw/GridViewport.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
int SelectedRow(int itemCount, int selectedIndex, int columns) {
    return itemCount <= 0 ? 0 : std::clamp(selectedIndex, 0, itemCount - 1) / columns;
}
} // namespace

GridViewport::GridViewport(const GridViewportSettings& settings)
    : _settings(settings) {
    if (settings.bounds.width <= 0 || settings.bounds.height <= 0 || settings.columns <= 0 ||
        settings.visibleRows <= 0 || settings.tileWidth <= 0 || settings.tileHeight <= 0 || settings.gapX < 0 ||
        settings.gapY < 0 || !std::isfinite(settings.scrollSpeed) || settings.scrollSpeed < 0.0f) {
        throw std::invalid_argument("Invalid grid viewport settings");
    }
}

void GridViewport::Reset(int itemCount, int selectedIndex) {
    const int last = std::max(0, RowCount(itemCount) - _settings.visibleRows);
    const int row = SelectedRow(itemCount, selectedIndex, _settings.columns);
    _scrollRows = static_cast<float>(std::clamp(row - _settings.visibleRows + 1, 0, last));
}

void GridViewport::Update(int itemCount, int selectedIndex, float deltaSeconds) {
    const int last = std::max(0, RowCount(itemCount) - _settings.visibleRows);
    _scrollRows = std::clamp(_scrollRows, 0.0f, static_cast<float>(last));
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds) || itemCount <= 0) return;
    const int row = SelectedRow(itemCount, selectedIndex, _settings.columns);
    const float target = std::clamp(_scrollRows, static_cast<float>(std::max(0, row - _settings.visibleRows + 1)),
                                    static_cast<float>(std::min(row, last)));
    const float step = _settings.scrollSpeed == 0.0f ? 1.0f : std::min(1.0f, deltaSeconds * _settings.scrollSpeed);
    _scrollRows += (target - _scrollRows) * step;
}

Rect GridViewport::TileArea(int index) const {
    if (index < 0) return {};
    const int row = index / _settings.columns;
    const int column = index % _settings.columns;
    const float y = static_cast<float>(_settings.bounds.y) +
                    (static_cast<float>(row) - _scrollRows) * static_cast<float>(_settings.tileHeight + _settings.gapY);
    return {_settings.bounds.x + column * (_settings.tileWidth + _settings.gapX), static_cast<int>(std::lround(y)),
            _settings.tileWidth, _settings.tileHeight};
}

std::optional<int> GridViewport::TileAt(Vec2 point, int itemCount) const {
    if (!_settings.bounds.Contains(point)) return std::nullopt;
    for (int i = 0; i < itemCount; ++i)
        if (TileArea(i).Contains(point)) return i;
    return std::nullopt;
}

int GridViewport::RowCount(int itemCount) const { return itemCount <= 0 ? 0 : 1 + (itemCount - 1) / _settings.columns; }
