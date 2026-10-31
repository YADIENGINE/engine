#pragma once
#include "Graphics/Gpu/Internals/GpuTypes.h"
#include "Graphics/Surface/TextureRef.h"

// Engine-side construction and reading of TextureRef.
struct TextureRefAccess {
    [[nodiscard]] static TextureRef Make(GpuTexture texture, int width, int height, float logicalWidth,
                                         float logicalHeight, bool flipY) {
        TextureRef ref;
        ref._index = texture.index;
        ref._generation = texture.generation;
        ref._width = width;
        ref._height = height;
        ref._logicalWidth = logicalWidth;
        ref._logicalHeight = logicalHeight;
        ref._flipY = flipY;
        return ref;
    }
    [[nodiscard]] static GpuTexture Texture(const TextureRef& ref) { return {ref._index, ref._generation}; }
    [[nodiscard]] static bool FlipY(const TextureRef& ref) { return ref._flipY; }
};
