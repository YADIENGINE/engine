#include "Graphics/Pixel/PixelCanvas.h"
#include <algorithm>
#include <format>
#include <stdexcept>
#include "Graphics/Pixel/Internals/PixelRaster.h"

std::uint32_t PixelHash(std::uint32_t a, std::uint32_t b) {
    std::uint32_t value = a * 2654435761U ^ (b + 0x9E3779B9U + (a << 6U) + (a >> 2U));
    value ^= value >> 15U;
    value *= 2246822519U;
    value ^= value >> 13U;
    return value;
}

PixelPartScope::~PixelPartScope() {
    if (_canvas) _canvas->_part = _previous;
}

PixelCanvas::PixelCanvas(int width, int height, const PixelCanvasStyle& style)
    : _width(width)
    , _height(height)
    , _style(style) {
    if (width < 0 || height < 0)
        throw std::invalid_argument(std::format("PixelCanvas: size {}x{} must not be negative", width, height));
    const auto cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    _inks.assign(cells, ClearInk);
    _parts.assign(cells, NoPart);
}

Ink PixelCanvas::Get(int x, int y) const {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return ClearInk;
    return _inks[Index(x, y)];
}

PixelPart PixelCanvas::PartAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return NoPart;
    return _parts[Index(x, y)];
}

void PixelCanvas::Set(int x, int y, Ink ink) {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    const std::size_t index = Index(x, y);
    _inks[index] = ink;
    _parts[index] = ink == ClearInk ? NoPart : _part;
}

PixelPartScope PixelCanvas::Part(PixelPart part) {
    const PixelPart previous = _part;
    _part = part;
    return PixelPartScope(*this, previous);
}

void PixelCanvas::Shade(const PixelMask& mask, const InkRamp& ramp, int bevel, bool edged) {
    if (edged) {
        const Ink edge = _style.edge != ClearInk ? _style.edge : ramp.deep;
        for (int y = 0; y < _height; ++y) {
            for (int x = 0; x < _width; ++x) {
                if (mask.Has(x, y) || Get(x, y) == ClearInk) continue;
                if (mask.Has(x - 1, y) || mask.Has(x + 1, y) || mask.Has(x, y - 1) || mask.Has(x, y + 1))
                    Set(x, y, edge);
            }
        }
    }
    const int lightX = _style.light.x;
    const int lightY = _style.light.y;
    // Whether the pixel 'distance' steps toward (sign 1) or away from (sign -1) the light is outside the mask.
    const auto open = [&](int x, int y, int sign, int distance) {
        return !mask.Has(x + sign * lightX * distance, y + sign * lightY * distance);
    };
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            if (!mask.Has(x, y)) continue;
            Ink ink = ramp.base;
            if (open(x, y, -1, bevel + 1) && (x + y) % 2 == 0) ink = ramp.shade;
            if (open(x, y, -1, bevel)) ink = ramp.shade;
            if (open(x, y, 1, bevel)) ink = ramp.lit;
            if (open(x, y, -1, 1)) ink = ramp.deep;
            if (open(x, y, 1, 1)) ink = ramp.shine;
            Set(x, y, ink);
        }
    }
}

void PixelCanvas::Poly(std::initializer_list<Vec2> points, const InkRamp& ramp, int bevel, bool edged) {
    Poly(std::span<const Vec2>(points.begin(), points.size()), ramp, bevel, edged);
}

void PixelCanvas::Poly(std::span<const Vec2> points, const InkRamp& ramp, int bevel, bool edged) {
    PixelMask mask(_width, _height);
    mask.Poly(points);
    Shade(mask, ramp, bevel, edged);
}

void PixelCanvas::Ellipse(float centerX, float centerY, float radiusX, float radiusY, const InkRamp& ramp, int bevel,
                          bool edged) {
    PixelMask mask(_width, _height);
    mask.Ellipse(centerX, centerY, radiusX, radiusY);
    Shade(mask, ramp, bevel, edged);
}

void PixelCanvas::Limb(float x0, float y0, float x1, float y1, float r0, float r1, const InkRamp& ramp, int bevel,
                       bool edged) {
    PixelMask mask(_width, _height);
    mask.Limb(x0, y0, x1, y1, r0, r1);
    Shade(mask, ramp, bevel, edged);
}

void PixelCanvas::Fill(const PixelMask& mask, Ink ink) {
    for (int y = 0; y < _height; ++y)
        for (int x = 0; x < _width; ++x)
            if (mask.Has(x, y)) Set(x, y, ink);
}

void PixelCanvas::FlatRect(int x, int y, int width, int height, Ink ink) {
    for (int row = std::max(y, 0); row < std::min(y + height, _height); ++row)
        for (int column = std::max(x, 0); column < std::min(x + width, _width); ++column)
            Set(column, row, ink);
}

void PixelCanvas::FlatLine(int x0, int y0, int x1, int y1, Ink ink) {
    PixelRaster::Line(x0, y0, x1, y1, [&](int x, int y) { Set(x, y, ink); });
}

void PixelCanvas::FlatPoly(std::initializer_list<Vec2> points, Ink ink) {
    FlatPoly(std::span<const Vec2>(points.begin(), points.size()), ink);
}

void PixelCanvas::FlatPoly(std::span<const Vec2> points, Ink ink) {
    PixelRaster::Polygon(points, _width, _height, [&](int x, int y) { Set(x, y, ink); });
}

void PixelCanvas::FlatEllipse(float centerX, float centerY, float radiusX, float radiusY, Ink ink) {
    PixelRaster::Ellipse(centerX, centerY, radiusX, radiusY, _width, _height, [&](int x, int y) { Set(x, y, ink); });
}

void PixelCanvas::Stamp(int x, int y, std::initializer_list<std::string_view> rows) {
    int row = y;
    for (const std::string_view line : rows) {
        int column = x;
        for (const char letter : line) {
            if (const auto ink = _style.legend.Find(letter)) Set(column, row, *ink);
            ++column;
        }
        ++row;
    }
}

void PixelCanvas::Speckle(Ink from, Ink to, int percent, std::uint32_t seed) {
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            if (Get(x, y) != from) continue;
            if (PixelHash(static_cast<std::uint32_t>(x * 4099 + y), seed) % 100U < static_cast<std::uint32_t>(percent))
                Set(x, y, to);
        }
    }
}

void PixelCanvas::Outline(Ink ink) {
    const std::vector<Ink> before = _inks;
    const auto filled = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= _width || y >= _height) return false;
        return before[Index(x, y)] != ClearInk;
    };
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            if (filled(x, y)) continue;
            if (filled(x - 1, y) || filled(x + 1, y) || filled(x, y - 1) || filled(x, y + 1)) Set(x, y, ink);
        }
    }
}

void PixelCanvas::Paste(const PixelCanvas& other, int x, int y) {
    for (int row = 0; row < other._height; ++row) {
        for (int column = 0; column < other._width; ++column) {
            const Ink ink = other.Get(column, row);
            const int tx = x + column;
            const int ty = y + row;
            if (ink == ClearInk || tx < 0 || ty < 0 || tx >= _width || ty >= _height) continue;
            const PixelPart part = other.PartAt(column, row);
            const std::size_t index = Index(tx, ty);
            _inks[index] = ink;
            _parts[index] = part != NoPart ? part : _part;
        }
    }
}

void PixelCanvas::FlipX() {
    for (int y = 0; y < _height; ++y) {
        std::reverse(_inks.begin() + static_cast<std::ptrdiff_t>(Index(0, y)),
                     _inks.begin() + static_cast<std::ptrdiff_t>(Index(0, y) + static_cast<std::size_t>(_width)));
        std::reverse(_parts.begin() + static_cast<std::ptrdiff_t>(Index(0, y)),
                     _parts.begin() + static_cast<std::ptrdiff_t>(Index(0, y) + static_cast<std::size_t>(_width)));
    }
}
