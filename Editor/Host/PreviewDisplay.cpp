#include "Editor/Host/PreviewDisplay.h"

PreviewDisplay::PreviewDisplay() = default;
PreviewDisplay::~PreviewDisplay() {
    if (_texture.id) UnloadTexture(_texture);
}
bool PreviewDisplay::Upload(int width, int height, std::span<const std::byte> pixels, std::uint64_t generation,
                            std::uint64_t sequence, double transferMs) {
    if (_texture.id && (_texture.width != width || _texture.height != height)) {
        UnloadTexture(_texture);
        _texture = {};
    }
    if (!_texture.id) {
        ::Image image{const_cast<std::byte*>(pixels.data()), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        _texture = LoadTextureFromImage(image);
        if (!_texture.id) {
            _image = {};
            return false;
        }
        SetTextureFilter(_texture, TEXTURE_FILTER_BILINEAR);
    } else
        UpdateTexture(_texture, pixels.data());
    _image = {static_cast<std::uint64_t>(_texture.id), width, height, generation, sequence, transferMs};
    return true;
}
EditorPreviewImage PreviewDisplay::Image() const { return _image; }
