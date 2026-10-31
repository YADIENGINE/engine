#pragma once
#include <optional>
#include <vector>
#include "Graphics/Gpu/Internals/GpuTypes.h"

// Skyline bottom-left rectangle packer for a fixed-size atlas page. Rectangles keep 'padding' empty pixels to their
// right and bottom (and the page's first row/column stay free) so filtering never bleeds between neighbours.
class AtlasPacker {
public:
    AtlasPacker(int width, int height, int padding);

    // Place for a width x height rectangle, or nullopt when the page has no room left.
    [[nodiscard]] std::optional<PixelRect> Insert(int width, int height);
    void Clear();
    [[nodiscard]] float Occupancy() const; // Used area / page area, padding included

private:
    struct Span {
        int x = 0;
        int y = 0; // Top of the free space above this span
        int width = 0;
    };
    int _width;
    int _height;
    int _padding;
    long long _used = 0;
    std::vector<Span> _skyline;
};
