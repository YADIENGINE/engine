#include "Graphics/Render/LegacyTexture.h"
#include "Graphics/Paint/Painter.hpp"
#include "Graphics/Surface/Surface.hpp"

Texture2D LegacyTexture(const Painter& paint, const Surface& surface) {
    const Float2 size = surface.Size();
    return {paint.LegacyTextureName(surface), static_cast<int>(size.x), static_cast<int>(size.y), 1,
            PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
