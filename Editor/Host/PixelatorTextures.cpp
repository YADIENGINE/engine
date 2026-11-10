#include "Editor/Host/PixelatorTextures.h"

namespace {
constexpr std::size_t MaxSlots = 8;
constexpr int MaxSide = 4096;
} // namespace

PixelatorTextures::~PixelatorTextures() {
    for (const auto& texture : _slots)
        if (texture.id) UnloadTexture(texture);
}

std::uint64_t PixelatorTextures::Upload(std::size_t slot, int width, int height, std::span<const Color4> pixels) {
    if (slot >= MaxSlots || width <= 0 || height <= 0 || width > MaxSide || height > MaxSide ||
        pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        return 0;
    static_assert(sizeof(Color4) == 4, "Color4 uploads as RGBA bytes");
    if (_slots.size() <= slot) _slots.resize(slot + 1);
    auto& texture = _slots[slot];
    if (texture.id && texture.width == width && texture.height == height) {
        UpdateTexture(texture, pixels.data());
        return texture.id;
    }
    if (texture.id) UnloadTexture(texture);
    Image image{const_cast<Color4*>(pixels.data()), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    texture = LoadTextureFromImage(image);
    if (!texture.id) return 0;
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    return texture.id;
}
