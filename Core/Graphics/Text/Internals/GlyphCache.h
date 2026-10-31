#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Text/Internals/AtlasPacker.h"
#include "Utilities/FloatRect.h"

class FontData;
struct GlyphRaster;

// A glyph in an atlas page; metrics in pixels of the size it was rasterized at.
struct CachedGlyph {
    GpuTexture page{};
    FloatRect uv;
    float left = 0.0f; // From the pen
    float top = 0.0f;  // From the baseline
    float width = 0.0f;
    float height = 0.0f;
    bool empty = true; // Nothing to draw (space)
};

// Rasterized glyphs on shared R8 atlas pages: coverage glyphs at the exact pixel size they are drawn at, and one
// signed-distance glyph per character (halos and outlines at any size). Pages that were not used in the current frame
// are recycled least-recently-used first once the budget is reached.
class GlyphCache {
public:
    static constexpr int DistanceSize = 32;      // Pixel size of distance glyphs
    static constexpr int DistancePadding = 16;   // Pixels of field around them: halos reach half the font size
    static constexpr float DistanceScale = 8.0f; // Byte change per pixel of distance (128 on the outline)

    explicit GlyphCache(GpuDevice& device, int pageSize = 1024, std::size_t pageBudget = 8);
    ~GlyphCache();

    void BeginFrame();
    [[nodiscard]] const CachedGlyph& Coverage(const FontData& font, int glyph, int pixelSize);
    [[nodiscard]] const CachedGlyph& Distance(const FontData& font, int glyph);
    [[nodiscard]] std::uint32_t RasterizedThisFrame() const { return _rasterized; }
    [[nodiscard]] std::size_t Pages() const { return _pages.size(); }

    // Pixel size a glyph drawn 'pixels' tall is rasterized at: exact up to ExactLimit, then in ~6% steps up to
    // MaxSize (larger text scales the largest raster).
    [[nodiscard]] static int RasterSize(float pixels);

private:
    struct Key {
        std::uint32_t font = 0;
        int glyph = 0;
        int size = 0; // 0: distance glyph
        friend bool operator==(const Key&, const Key&) = default;
    };
    struct KeyHash {
        std::size_t operator()(const Key& key) const noexcept;
    };
    struct Entry {
        CachedGlyph glyph;
        std::size_t page = 0;
    };
    struct Page {
        GpuTexture texture;
        AtlasPacker packer;
        std::uint64_t lastUsed = 0;
        std::vector<Key> keys;
    };
    const CachedGlyph& Insert(const Key& key, const GlyphRaster& raster);
    [[nodiscard]] std::size_t PageWithRoom(int width, int height, PixelRect& place);

    GpuDevice& _device;
    int _pageSize;
    std::size_t _pageBudget;
    std::vector<Page> _pages;
    std::unordered_map<Key, Entry, KeyHash> _entries;
    std::uint64_t _frame = 1;
    std::uint32_t _rasterized = 0;

public:
    GlyphCache(const GlyphCache&) = delete;
    GlyphCache& operator=(const GlyphCache&) = delete;
};
