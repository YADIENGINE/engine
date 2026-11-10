#pragma once
#include <raylib.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "Utilities/Color4.h"

// The Pixelator panel's textures, by slot: RGBA pixels uploaded into GL textures drawn with nearest filtering and
// repeating (the checker tiles itself). A slot of the same size is updated in place.
class PixelatorTextures {
public:
    PixelatorTextures() = default;
    ~PixelatorTextures();
    // The slot's GL texture after the upload; 0 when the size is invalid or the texture could not be made.
    [[nodiscard]] std::uint64_t Upload(std::size_t slot, int width, int height, std::span<const Color4> pixels);

private:
    std::vector<Texture2D> _slots;

public:
    PixelatorTextures(const PixelatorTextures&) = delete;
    PixelatorTextures& operator=(const PixelatorTextures&) = delete;
};
