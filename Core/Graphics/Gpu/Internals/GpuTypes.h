#pragma once
#include <cstdint>
#include <optional>
#include "Utilities/Float2.h"

// GPU resources are addressed by generational handles: a slot index plus the generation the slot had when the
// resource was created. A destroyed resource's handle never matches a later resource in the same slot.
template <class Tag> struct GpuHandle {
    std::uint32_t index = 0; // 0: null handle
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool Valid() const { return index != 0; }
    friend constexpr bool operator==(GpuHandle, GpuHandle) = default;
};
struct GpuTextureTag;
struct GpuTargetTag;
using GpuTexture = GpuHandle<GpuTextureTag>;
using GpuTarget = GpuHandle<GpuTargetTag>; // Render target: one or two colour attachments

// R8: coverage (glyph atlases); Rgba8: premultiplied colour; Rgba16F: high-precision effect buffers.
enum class GpuFormat { R8, Rgba8, Rgba16F };
enum class GpuFilter { Nearest, Linear };

struct GpuTextureDesc {
    int width = 0;
    int height = 0;
    GpuFormat format = GpuFormat::Rgba8;
    GpuFilter filter = GpuFilter::Linear;
    const char* label = nullptr; // Debug name (RenderDoc); the pointer is only read during creation
};

struct GpuTargetDesc {
    int width = 0;
    int height = 0;
    GpuFormat format = GpuFormat::Rgba8;
    GpuFilter filter = GpuFilter::Linear;
    bool emission = false; // Second attachment for bloom sources (same size and format)
    const char* label = nullptr;
};

// Device pixels, origin top-left of the pass's viewport.
struct PixelRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    friend constexpr bool operator==(const PixelRect&, const PixelRect&) = default;
};

struct PremultipliedColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
    friend constexpr bool operator==(const PremultipliedColor&, const PremultipliedColor&) = default;
};

// Blend states. Every colour in the pipeline is premultiplied, so Premultiplied covers both normal (alpha) and
// additive drawing (alpha 0).
enum class GpuBlend { Premultiplied, Multiply, Screen, Replace };

enum class GpuProgram { Paint, BloomDownsample, BloomUpsample, Compose };

struct GpuPassDesc {
    GpuTarget target{};                      // Null: the window's back buffer
    PixelRect viewport;                      // Within the target, device pixels
    Float2 viewSize;                         // Logical size the viewport covers (paint coordinates)
    std::optional<PremultipliedColor> clear; // Clears the whole viewport (all attachments) when set
};

// Uniforms of the post-processing programs; Paint reads only the pass's view size.
struct PostUniforms {
    Float2 sourceTexel;          // 1 / source texture size
    float bloomIntensity = 0.0f; // Compose: bloom added on top of the colour
    float bloomSpread = 1.0f;    // Bloom passes: sample offset multiplier
    float vignette = 0.0f;       // Compose: corner darkening 0..1
    float scanlines = 0.0f;      // Compose: scanline darkening 0..1
    float scanlineCount = 0.0f;  // Compose: scanlines over the full height
    float aberration = 0.0f;     // Compose: chromatic offset in source texels at the edges
    float time = 0.0f;
};

// Where an upload starts inside the device's streaming buffers.
struct GpuGeometry {
    std::int64_t baseVertex = 0; // Added to every index of this upload (indices are relative to it)
    std::uint32_t firstIndex = 0;
};

// Native identifiers of a render target, for platform code that reads it directly (the editor's frame capture).
struct GpuNativeTarget {
    unsigned framebuffer = 0;
    unsigned colorTexture = 0;
    int width = 0;
    int height = 0;
    friend constexpr bool operator==(const GpuNativeTarget&, const GpuNativeTarget&) = default;
};
