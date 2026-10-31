#pragma once
#include <cstdint>

// Non-owning reference to a GPU texture (a surface's image, a static texture). Cheap to copy; valid while its owner
// lives and keeps its size. Painter::Image checks it: a reference to a destroyed texture throws.
class TextureRef {
public:
    TextureRef() = default;

    [[nodiscard]] bool Valid() const { return _index != 0; }
    [[nodiscard]] int Width() const { return _width; }   // Device pixels
    [[nodiscard]] int Height() const { return _height; } // Device pixels
    // Logical size the texture was drawn at (surfaces) or its pixel size (static textures).
    [[nodiscard]] float LogicalWidth() const { return _logicalWidth; }
    [[nodiscard]] float LogicalHeight() const { return _logicalHeight; }

    friend bool operator==(const TextureRef&, const TextureRef&) = default;

private:
    friend struct TextureRefAccess;
    std::uint32_t _index = 0;
    std::uint32_t _generation = 0;
    int _width = 0;
    int _height = 0;
    float _logicalWidth = 0.0f;
    float _logicalHeight = 0.0f;
    bool _flipY = false; // Rendered images are stored bottom row first
};
