#include "Graphics/Text/Internals/AtlasPacker.h"
#include <algorithm>
#include <limits>

AtlasPacker::AtlasPacker(int width, int height, int padding)
    : _width(width)
    , _height(height)
    , _padding(padding) {
    Clear();
}

void AtlasPacker::Clear() {
    _skyline.assign(1, Span{_padding, _padding, _width - _padding});
    _used = 0;
}

float AtlasPacker::Occupancy() const {
    return static_cast<float>(_used) / static_cast<float>(static_cast<long long>(_width) * _height);
}

std::optional<PixelRect> AtlasPacker::Insert(int width, int height) {
    if (width <= 0 || height <= 0) return PixelRect{0, 0, 0, 0};
    const int paddedWidth = width + _padding;
    const int paddedHeight = height + _padding;
    int bestY = std::numeric_limits<int>::max();
    int bestWaste = std::numeric_limits<int>::max();
    std::size_t bestIndex = _skyline.size();
    for (std::size_t i = 0; i < _skyline.size(); ++i) {
        const int x = _skyline[i].x;
        if (x + paddedWidth > _width) break;
        // The rectangle rests on the highest span it covers.
        int y = 0;
        int covered = 0;
        std::size_t j = i;
        while (covered < paddedWidth && j < _skyline.size()) {
            y = std::max(y, _skyline[j].y);
            covered += _skyline[j].width;
            ++j;
        }
        if (covered < paddedWidth || y + paddedHeight > _height) continue;
        int waste = 0;
        covered = 0;
        for (std::size_t k = i; k < j; ++k) {
            const int overlap = std::min(_skyline[k].width, paddedWidth - covered);
            waste += (y - _skyline[k].y) * overlap;
            covered += overlap;
        }
        if (y < bestY || (y == bestY && waste < bestWaste)) {
            bestY = y;
            bestWaste = waste;
            bestIndex = i;
        }
    }
    if (bestIndex == _skyline.size()) return std::nullopt;

    const int x = _skyline[bestIndex].x;
    const Span placed{x, bestY + paddedHeight, paddedWidth};
    // Spans under the new rectangle shrink or disappear.
    std::size_t i = bestIndex;
    int remaining = paddedWidth;
    while (i < _skyline.size() && remaining > 0) {
        Span& span = _skyline[i];
        if (span.width <= remaining) {
            remaining -= span.width;
            _skyline.erase(_skyline.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            span.x += remaining;
            span.width -= remaining;
            remaining = 0;
        }
    }
    _skyline.insert(_skyline.begin() + static_cast<std::ptrdiff_t>(bestIndex), placed);
    // Neighbours at the same height merge.
    for (std::size_t k = 0; k + 1 < _skyline.size();) {
        if (_skyline[k].y == _skyline[k + 1].y) {
            _skyline[k].width += _skyline[k + 1].width;
            _skyline.erase(_skyline.begin() + static_cast<std::ptrdiff_t>(k + 1));
        } else {
            ++k;
        }
    }
    _used += static_cast<long long>(paddedWidth) * paddedHeight;
    return PixelRect{x, bestY, width, height};
}
