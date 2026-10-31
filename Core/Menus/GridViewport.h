#pragma once
#include <optional>
#include "Utilities/FloatRect.h"

struct GridViewportSettings {
    FloatRect bounds;
    int columns = 1;
    int visibleRows = 1;
    float tileWidth = 1.0f;
    float tileHeight = 1.0f;
    float gapX = 0.0f;
    float gapY = 0.0f;
    float scrollSpeed = 0.0f; // zero snaps immediately
};

// Scroll position and tile geometry; selection, content, input and drawing belong to the caller.
class GridViewport {
public:
    explicit GridViewport(const GridViewportSettings& settings);
    void Reset(int itemCount, int selectedIndex);
    void Update(int itemCount, int selectedIndex, float deltaSeconds);
    [[nodiscard]] FloatRect TileArea(int index) const;
    // Uses the current count even when input changes content before the next Update.
    [[nodiscard]] std::optional<int> TileAt(Vec2 point, int itemCount) const;
    [[nodiscard]] int RowCount(int itemCount) const;
    [[nodiscard]] float ScrollRows() const { return _scrollRows; }

private:
    GridViewportSettings _settings;
    float _scrollRows = 0.0f;
};
