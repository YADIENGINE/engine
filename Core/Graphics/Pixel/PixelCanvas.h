#pragma once
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <utility>
#include <vector>
#include "Graphics/Pixel/PixelLegend.h"
#include "Graphics/Pixel/PixelMask.h"
#include "Graphics/Pixel/PixelPalette.h"
#include "Utilities/Geometry/Vec2.h"

// Which piece of a drawing a pixel belongs to (helmet, shield, cape): a game's own enum values, 0 for none. A sprite
// keeps them, so it can later break along them.
using PixelPart = std::uint8_t;
inline constexpr PixelPart NoPart = 0;

class PixelCanvas;

// The part following drawing belongs to, until the scope ends (PixelCanvas::Part). Keep it in a local variable.
class [[nodiscard]] PixelPartScope {
public:
    PixelPartScope(PixelPartScope&& other) noexcept
        : _canvas(std::exchange(other._canvas, nullptr))
        , _previous(other._previous) {}
    ~PixelPartScope();

private:
    friend class PixelCanvas;
    PixelPartScope(PixelCanvas& canvas, PixelPart previous)
        : _canvas(&canvas)
        , _previous(previous) {}
    PixelCanvas* _canvas; // Null once moved from
    PixelPart _previous;

public:
    PixelPartScope(const PixelPartScope&) = delete;
    PixelPartScope& operator=(const PixelPartScope&) = delete;
    PixelPartScope& operator=(PixelPartScope&&) = delete;
};

// How a canvas shades and stamps, usually one value per game or character.
struct PixelCanvasStyle {
    // Where light comes from, one step per axis: (-1, -1) is the top left. Volumes are bright on that side.
    Vec2 light{-1, -1};
    // The ink of the line a shaded volume draws over what is already behind it (edged); ClearInk: the ramp's deep ink.
    Ink edge = ClearInk;
    PixelLegend legend{};
};

// A grid of inks drawn once, in code, then made into a PixelSprite. Draw shapes and let the canvas shade them rather
// than placing pixels one by one: a mask filled with an InkRamp gets a dark side, a lit side and a rim from the light
// direction; Stamp is for small hand-placed details; Outline rings the whole drawing. Every write also records the
// current part (Part). Coordinates outside the grid are ignored; everything is deterministic.
//
//     PixelCanvas c(32, 40, style);
//     { auto helmet = c.Part(KnightPart::Helmet); c.Ellipse(16, 9, 7, 8, Steel); }
//     c.Limb(12, 18, 8, 30, 3, 2, Cloth);
//     c.Stamp(14, 7, {"1.1", ".a."});
//     c.Outline(Black);
//     const PixelSprite sprite(c, {16, 40});
class PixelCanvas {
public:
    // A transparent canvas; a negative size is invalid_argument.
    PixelCanvas(int width, int height, const PixelCanvasStyle& style = {});

    [[nodiscard]] int Width() const { return _width; }
    [[nodiscard]] int Height() const { return _height; }
    [[nodiscard]] Ink Get(int x, int y) const;
    [[nodiscard]] PixelPart PartAt(int x, int y) const;
    // Writes one pixel with the current part (ClearInk clears the part too).
    void Set(int x, int y, Ink ink);
    [[nodiscard]] std::span<const Ink> Inks() const { return _inks; }
    [[nodiscard]] std::span<const PixelPart> Parts() const { return _parts; }

    void SetLight(int dx, int dy) { _style.light = {dx, dy}; }
    void SetEdge(Ink ink) { _style.edge = ink; }
    void SetLegend(const PixelLegend& legend) { _style.legend = legend; }
    [[nodiscard]] const PixelCanvasStyle& Style() const { return _style; }

    // Following writes belong to 'part' until the scope ends.
    [[nodiscard]] PixelPartScope Part(PixelPart part);
    [[nodiscard]] PixelPart CurrentPart() const { return _part; }

    // --- Shaded volumes ------------------------------------------------------------------------------------------
    // Fills 'mask' with 'ramp': 'bevel' pixels toward the light are lit, away from it shaded, the outermost pixel on
    // each side shine / deep, with a dither between shade and base. 'edged' first draws the edge ink where the mask
    // touches pixels already drawn, separating it from what is behind.
    void Shade(const PixelMask& mask, const InkRamp& ramp, int bevel = 2, bool edged = true);
    void Poly(std::initializer_list<Vec2> points, const InkRamp& ramp, int bevel = 2, bool edged = true);
    void Poly(std::span<const Vec2> points, const InkRamp& ramp, int bevel = 2, bool edged = true);
    void Ellipse(float centerX, float centerY, float radiusX, float radiusY, const InkRamp& ramp, int bevel = 2,
                 bool edged = true);
    void Limb(float x0, float y0, float x1, float y1, float r0, float r1, const InkRamp& ramp, int bevel = 1,
              bool edged = true);

    // --- Flat marks ----------------------------------------------------------------------------------------------
    void Fill(const PixelMask& mask, Ink ink);
    void FlatRect(int x, int y, int width, int height, Ink ink);
    void FlatLine(int x0, int y0, int x1, int y1, Ink ink);
    void FlatPoly(std::initializer_list<Vec2> points, Ink ink);
    void FlatPoly(std::span<const Vec2> points, Ink ink);
    void FlatEllipse(float centerX, float centerY, float radiusX, float radiusY, Ink ink);

    // --- Details -------------------------------------------------------------------------------------------------
    // Hand-placed pixels, one string per row from (x, y), read through the style's legend ('.' keeps).
    void Stamp(int x, int y, std::initializer_list<std::string_view> rows);
    // Replaces about 'percent'% of one ink's pixels with another, the same pixels for the same seed.
    void Speckle(Ink from, Ink to, int percent, std::uint32_t seed = 1);
    // A one-pixel ring around everything drawn.
    void Outline(Ink ink);
    // Copies the drawn pixels of 'other' with its top left at (x, y); its parts come along (NoPart: the current part).
    void Paste(const PixelCanvas& other, int x, int y);
    // Mirrors the canvas left to right.
    void FlipX();

private:
    friend class PixelPartScope;
    [[nodiscard]] std::size_t Index(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) + static_cast<std::size_t>(x);
    }
    int _width;
    int _height;
    PixelCanvasStyle _style;
    PixelPart _part = NoPart;
    std::vector<Ink> _inks;
    std::vector<PixelPart> _parts;
};

// Stable noise for placing details by position: the same inputs always give the same number.
[[nodiscard]] std::uint32_t PixelHash(std::uint32_t a, std::uint32_t b = 0);
