#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include "Graphics/Pixel/PixelCanvas.h"
#include "Utilities/Geometry/Vec2.h"

// A finished pixel drawing: inks and parts, frozen from a PixelCanvas, drawn with Painter::Sprite in any palette.
// Cheap to copy (copies share the pixels). The renderer uploads it on first draw and keeps it while it is drawn.
class PixelSprite {
public:
    // An empty sprite (draws nothing).
    PixelSprite() = default;
    // 'pivot' is the pixel corner that lands on the point the sprite is drawn at (usually the feet: {x, height}).
    explicit PixelSprite(const PixelCanvas& canvas, Vec2 pivot = {});

    [[nodiscard]] bool Empty() const { return !_data || _data->width == 0 || _data->height == 0; }
    [[nodiscard]] int Width() const { return _data ? _data->width : 0; }
    [[nodiscard]] int Height() const { return _data ? _data->height : 0; }
    [[nodiscard]] Vec2 Pivot() const { return _data ? _data->pivot : Vec2{}; }
    [[nodiscard]] Ink At(int x, int y) const;
    [[nodiscard]] PixelPart PartAt(int x, int y) const;
    // Rows top to bottom.
    [[nodiscard]] std::span<const Ink> Inks() const;
    [[nodiscard]] std::span<const PixelPart> Parts() const;
    // Unique per made sprite, shared by its copies; 0 for an empty sprite. The renderer's cache key.
    [[nodiscard]] std::uint64_t Id() const { return _data ? _data->id : 0; }

private:
    struct Data {
        int width = 0;
        int height = 0;
        Vec2 pivot;
        std::uint64_t id = 0;
        std::vector<Ink> inks;
        std::vector<PixelPart> parts;
    };
    std::shared_ptr<const Data> _data;
};
