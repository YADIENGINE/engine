#pragma once
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>
#include "Utilities/Geometry/Vec2.h"

// Which pixels of a grid a shape covers, before they get inks (PixelCanvas::Shade, Fill). Pixel (x, y) covers
// x..x+1, y..y+1 and its centre is (x + 0.5, y + 0.5): a shape takes a pixel when it covers that centre. Anything
// outside the grid is ignored.
class PixelMask {
public:
    // A grid with nothing set; a negative size is invalid_argument.
    PixelMask(int width, int height);

    // Polygon with corners on pixel corners, either winding; concave is fine. Fewer than three points adds nothing.
    void Poly(std::initializer_list<Vec2> points);
    void Poly(std::span<const Vec2> points);
    void Ellipse(float centerX, float centerY, float radiusX, float radiusY);
    // A tapered capsule from (x0, y0) to (x1, y1), radius r0 at the start and r1 at the end: arms, legs, tails.
    void Limb(float x0, float y0, float x1, float y1, float r0, float r1);
    void Rect(int x, int y, int width, int height);
    // A one-pixel line, both ends included (Bresenham).
    void Line(int x0, int y0, int x1, int y1);
    // Clears a rectangle.
    void Cut(int x, int y, int width, int height);
    // Adds, removes, or keeps only what 'other' covers. Sizes must match (invalid_argument).
    void Add(const PixelMask& other);
    void Remove(const PixelMask& other);
    void Keep(const PixelMask& other);
    void Set(int x, int y, bool covered);

    [[nodiscard]] bool Has(int x, int y) const;
    [[nodiscard]] int Width() const { return _width; }
    [[nodiscard]] int Height() const { return _height; }
    [[nodiscard]] int Count() const;

private:
    void RequireSameSize(const PixelMask& other, const char* what) const;
    int _width;
    int _height;
    std::vector<std::uint8_t> _bits;
};
