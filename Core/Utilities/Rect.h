#pragma once
#include "Utilities/Vec2.h"

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    [[nodiscard]] constexpr int Right() const { return x + width; }
    [[nodiscard]] constexpr int Bottom() const { return y + height; }
    [[nodiscard]] constexpr int CenterX() const { return x + width / 2; }
    [[nodiscard]] constexpr int CenterY() const { return y + height / 2; }
    [[nodiscard]] constexpr bool Contains(Vec2 point) const {
        return point.x >= x && point.x < Right() && point.y >= y && point.y < Bottom();
    }
    [[nodiscard]] constexpr bool ContainsCircle(Vec2 point) const {
        const int radius = (width < height ? width : height) / 2;
        const int dx = point.x - CenterX();
        const int dy = point.y - CenterY();
        return dx * dx + dy * dy <= radius * radius;
    }
};
