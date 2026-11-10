#pragma once
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>
#include "Utilities/Color4.h"

// A pixel sprite stores inks, not colours: an ink is an index into a palette, so one sprite shows in any palette
// (a hit flash, an enemy's own colours, night). Ink 0 is always transparent.
using Ink = std::uint8_t;
inline constexpr Ink ClearInk = 0;
inline constexpr int MaxInks = 256; // Ink 0 plus 255 colours

// Five inks of one material from dark to light, for shaded volumes (PixelCanvas::Shade): the side away from the light
// gets 'deep' and 'shade', the middle 'base', the lit side 'lit' and 'shine'. Inks may repeat.
struct InkRamp {
    Ink deep = ClearInk;
    Ink shade = ClearInk;
    Ink base = ClearInk;
    Ink lit = ClearInk;
    Ink shine = ClearInk;
    bool operator==(const InkRamp&) const = default;
};

// The colour of every ink. A value: copy it to make a variant (PixelPalette::Mixed for a flash, Set for an enemy's
// accent). Inks are numbered from 1 in the order they are added; ink 0 stays transparent.
//     enum : Ink { Night = 1, Stone, Moss, Lamp };
//     const PixelPalette Dungeon{Rgb(0x10141b), Rgb(0x394553), Rgb(0x4f6b3a), Rgb(0xf0a030)};
class PixelPalette {
public:
    PixelPalette();
    // Inks 1, 2, 3, ... in order. More than 255 colours is invalid_argument.
    PixelPalette(std::initializer_list<Color4> colors);
    explicit PixelPalette(std::span<const Color4> colors);

    // The next ink with 'color'; invalid_argument when all 255 are taken.
    Ink Add(Color4 color);
    // Changes an ink that exists (1..Count()-1); otherwise invalid_argument.
    void Set(Ink ink, Color4 color);
    // An ink's colour; inks the palette does not have are transparent.
    [[nodiscard]] Color4 operator[](Ink ink) const;
    // Inks including the transparent ink 0.
    [[nodiscard]] int Count() const { return static_cast<int>(_colors.size()); }
    [[nodiscard]] std::span<const Color4> Colors() const { return _colors; }

    // Every ink moved toward 'color' by 'amount' (0..1), its alpha kept: a whole-body flash.
    [[nodiscard]] PixelPalette Mixed(Color4 color, float amount) const;

    // Changes whenever the colours change; copies share it (the renderer uploads each revision once).
    [[nodiscard]] std::uint64_t Revision() const { return _revision; }

private:
    void Touch();
    std::vector<Color4> _colors;
    std::uint64_t _revision = 0;
};
