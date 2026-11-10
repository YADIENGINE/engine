#pragma once
#include <cstddef>
#include <optional>
#include <vector>
#include "Graphics/Pixel/PixelSprite.h"

// Sprites made the first time they are asked for and kept: a character's poses x animation frames, numbered by the
// game. Not thread-safe (the game thread draws).
//     PixelSprite PaintKnight(std::size_t index) { ... pose = index / 2, frame = index % 2 ... }
//     const PixelSpriteSet knight(PoseCount * 2, &PaintKnight);
//     paint.Sprite(knight[pose * 2 + frame], palette, feet);
class PixelSpriteSet {
public:
    using Make = PixelSprite (*)(std::size_t index);
    // 'make' must not be null (invalid_argument).
    PixelSpriteSet(std::size_t count, Make make);

    // Makes the sprite on first use; an index past Count() is out_of_range.
    [[nodiscard]] const PixelSprite& operator[](std::size_t index) const;
    [[nodiscard]] std::size_t Count() const { return _sprites.size(); }

private:
    Make _make;
    mutable std::vector<std::optional<PixelSprite>> _sprites;
};
