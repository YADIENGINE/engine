#include "Utilities/Random/Scatter.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
constexpr double MaxCells = 4194304.0; // 1 << 22: 16 MB of cell indices

// Cell of 'point' along one axis; 'point' is inside the area.
int CellOf(float offset, float cell, int count) { return std::min(static_cast<int>(offset / cell), count - 1); }

// An offset at least 'spacing' and under twice 'spacing' long, uniform over that ring. Drawn in the enclosing square
// and kept when it lands in the ring (about 6 tries in 10), so no sine or cosine is involved.
Float2 RingOffset(float spacing, Rng& rng) {
    const float inner = spacing * spacing;
    const float outer = 4.0f * inner;
    for (;;) {
        const Float2 offset{rng.Float(-2.0f * spacing, 2.0f * spacing), rng.Float(-2.0f * spacing, 2.0f * spacing)};
        const float distance = LengthSquared(offset);
        if (distance >= inner && distance < outer) return offset;
    }
}
} // namespace

std::vector<Float2> ScatterPoints(const FloatRect& area, float spacing, Rng& rng, int attempts) {
    std::vector<Float2> points;
    if (area.Empty() || !(spacing > 0.0f) || !std::isfinite(spacing) || !std::isfinite(area.width) ||
        !std::isfinite(area.height))
        return points;
    // A cell's diagonal is the spacing, so a cell holds at most one point.
    const float cell = spacing / std::sqrt(2.0f);
    const double columnsWide = std::ceil(static_cast<double>(area.width) / cell);
    const double rowsHigh = std::ceil(static_cast<double>(area.height) / cell);
    if (columnsWide * rowsHigh > MaxCells) return points;
    const int columns = std::max(1, static_cast<int>(columnsWide));
    const int rows = std::max(1, static_cast<int>(rowsHigh));
    attempts = std::max(attempts, 1);
    std::vector<int> cells(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), -1);
    const float spacingSquared = spacing * spacing;

    std::vector<std::size_t> active;
    const auto add = [&](Float2 point) {
        const int column = CellOf(point.x - area.x, cell, columns);
        const int row = CellOf(point.y - area.y, cell, rows);
        cells[static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(column)] =
            static_cast<int>(points.size());
        active.push_back(points.size());
        points.push_back(point);
    };
    const auto fits = [&](Float2 point) {
        if (!area.Contains(point)) return false;
        const int column = CellOf(point.x - area.x, cell, columns);
        const int row = CellOf(point.y - area.y, cell, rows);
        for (int y = std::max(row - 2, 0); y <= std::min(row + 2, rows - 1); ++y)
            for (int x = std::max(column - 2, 0); x <= std::min(column + 2, columns - 1); ++x) {
                const int other = cells[static_cast<std::size_t>(y) * static_cast<std::size_t>(columns) +
                                        static_cast<std::size_t>(x)];
                if (other >= 0 && LengthSquared(points[static_cast<std::size_t>(other)] - point) < spacingSquared)
                    return false;
            }
        return true;
    };

    add({area.x + rng.Float01() * area.width, area.y + rng.Float01() * area.height});
    while (!active.empty()) {
        const std::size_t slot = rng.Index(active.size());
        const Float2 from = points[active[slot]];
        bool placed = false;
        for (int attempt = 0; attempt < attempts && !placed; ++attempt) {
            const Float2 candidate = from + RingOffset(spacing, rng);
            if (fits(candidate)) {
                add(candidate);
                placed = true;
            }
        }
        if (!placed) {
            // Done: no room around it. Swapping the last one in keeps the order a function of the generator alone.
            active[slot] = active.back();
            active.pop_back();
        }
    }
    return points;
}
