#include "Graphics/Pixel/PixelSprite.h"
#include <atomic>

namespace {
std::uint64_t NextSpriteId() {
    static std::atomic<std::uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}
} // namespace

PixelSprite::PixelSprite(const PixelCanvas& canvas, Vec2 pivot) {
    auto data = std::make_shared<Data>();
    data->width = canvas.Width();
    data->height = canvas.Height();
    data->pivot = pivot;
    data->id = NextSpriteId();
    data->inks.assign(canvas.Inks().begin(), canvas.Inks().end());
    data->parts.assign(canvas.Parts().begin(), canvas.Parts().end());
    _data = std::move(data);
}

Ink PixelSprite::At(int x, int y) const {
    if (!_data || x < 0 || y < 0 || x >= _data->width || y >= _data->height) return ClearInk;
    return _data->inks[static_cast<std::size_t>(y) * static_cast<std::size_t>(_data->width) +
                       static_cast<std::size_t>(x)];
}

PixelPart PixelSprite::PartAt(int x, int y) const {
    if (!_data || x < 0 || y < 0 || x >= _data->width || y >= _data->height) return NoPart;
    return _data->parts[static_cast<std::size_t>(y) * static_cast<std::size_t>(_data->width) +
                        static_cast<std::size_t>(x)];
}

std::span<const Ink> PixelSprite::Inks() const { return _data ? std::span<const Ink>(_data->inks) : std::span<const Ink>(); }

std::span<const PixelPart> PixelSprite::Parts() const {
    return _data ? std::span<const PixelPart>(_data->parts) : std::span<const PixelPart>();
}
