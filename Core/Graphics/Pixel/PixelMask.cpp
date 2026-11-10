#include "Graphics/Pixel/PixelMask.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include "Graphics/Pixel/Internals/PixelRaster.h"

PixelMask::PixelMask(int width, int height)
    : _width(width)
    , _height(height) {
    if (width < 0 || height < 0)
        throw std::invalid_argument(std::format("PixelMask: size {}x{} must not be negative", width, height));
    _bits.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
}

void PixelMask::Set(int x, int y, bool covered) {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    _bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) + static_cast<std::size_t>(x)] = covered;
}

bool PixelMask::Has(int x, int y) const {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return false;
    return _bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) + static_cast<std::size_t>(x)] != 0;
}

int PixelMask::Count() const { return static_cast<int>(std::ranges::count(_bits, std::uint8_t{1})); }

void PixelMask::Poly(std::initializer_list<Vec2> points) { Poly(std::span<const Vec2>(points.begin(), points.size())); }

void PixelMask::Poly(std::span<const Vec2> points) {
    PixelRaster::Polygon(points, _width, _height, [this](int x, int y) { Set(x, y, true); });
}

void PixelMask::Ellipse(float centerX, float centerY, float radiusX, float radiusY) {
    PixelRaster::Ellipse(centerX, centerY, radiusX, radiusY, _width, _height, [this](int x, int y) { Set(x, y, true); });
}

void PixelMask::Limb(float x0, float y0, float x1, float y1, float r0, float r1) {
    PixelRaster::Limb(x0, y0, x1, y1, r0, r1, _width, _height, [this](int x, int y) { Set(x, y, true); });
}

void PixelMask::Rect(int x, int y, int width, int height) {
    for (int row = std::max(y, 0); row < std::min(y + height, _height); ++row)
        for (int column = std::max(x, 0); column < std::min(x + width, _width); ++column)
            Set(column, row, true);
}

void PixelMask::Line(int x0, int y0, int x1, int y1) {
    PixelRaster::Line(x0, y0, x1, y1, [this](int x, int y) { Set(x, y, true); });
}

void PixelMask::Cut(int x, int y, int width, int height) {
    for (int row = std::max(y, 0); row < std::min(y + height, _height); ++row)
        for (int column = std::max(x, 0); column < std::min(x + width, _width); ++column)
            Set(column, row, false);
}

void PixelMask::RequireSameSize(const PixelMask& other, const char* what) const {
    if (other._width != _width || other._height != _height)
        throw std::invalid_argument(std::format("PixelMask::{}: sizes differ ({}x{} and {}x{})", what, _width, _height,
                                                other._width, other._height));
}

void PixelMask::Add(const PixelMask& other) {
    RequireSameSize(other, "Add");
    for (std::size_t i = 0; i < _bits.size(); ++i)
        _bits[i] = static_cast<std::uint8_t>(_bits[i] | other._bits[i]);
}

void PixelMask::Remove(const PixelMask& other) {
    RequireSameSize(other, "Remove");
    for (std::size_t i = 0; i < _bits.size(); ++i)
        if (other._bits[i] != 0) _bits[i] = 0;
}

void PixelMask::Keep(const PixelMask& other) {
    RequireSameSize(other, "Keep");
    for (std::size_t i = 0; i < _bits.size(); ++i)
        _bits[i] = static_cast<std::uint8_t>(_bits[i] & other._bits[i]);
}
