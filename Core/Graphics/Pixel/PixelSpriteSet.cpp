#include "Graphics/Pixel/PixelSpriteSet.h"
#include <format>
#include <stdexcept>

PixelSpriteSet::PixelSpriteSet(std::size_t count, Make make)
    : _make(make)
    , _sprites(count) {
    if (!make) throw std::invalid_argument("PixelSpriteSet: no function to make the sprites");
}

const PixelSprite& PixelSpriteSet::operator[](std::size_t index) const {
    if (index >= _sprites.size())
        throw std::out_of_range(std::format("PixelSpriteSet: sprite {} of {}", index, _sprites.size()));
    auto& slot = _sprites[index];
    if (!slot) slot = _make(index);
    return *slot;
}
