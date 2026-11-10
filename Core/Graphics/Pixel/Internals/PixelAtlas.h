#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Text/Internals/AtlasPacker.h"
#include "Utilities/Geometry/FloatRect.h"

class PixelPalette;
class PixelSprite;

// GPU side of pixel sprites (Painter::Sprite): sprites' inks on shared R8 atlas pages, sampled nearest, and palettes as
// rows of one RGBA texture (premultiplied), so the paint shader looks each ink up in the palette it is drawn with.
// Pages and rows not used in the current frame are recycled least-recently-used first once the budget is reached
// (an upload during a frame never overwrites something that frame already drew). Owned by the renderer.
class PixelAtlas {
public:
    static constexpr int PaletteWidth = 256; // One texel per ink

    explicit PixelAtlas(GpuDevice& device, int pageSize = 2048, std::size_t pageBudget = 8, int paletteRows = 256);
    ~PixelAtlas();

    void BeginFrame();

    struct Placed {
        GpuTexture page{};
        FloatRect uv; // Of the sprite's pixels on its page
    };
    // The sprite's place, uploading it on first use. The sprite must fit a page (PageSize) and not be empty.
    [[nodiscard]] const Placed& Sprite(const PixelSprite& sprite);

    struct Row {
        GpuTexture texture{};
        float v = 0.0f; // Texture coordinate of the row's centre
    };
    // The palette's row, uploading this revision on first use. More palettes in one frame than rows: runtime_error.
    [[nodiscard]] Row Palette(const PixelPalette& palette);

    [[nodiscard]] int PageSize() const { return _pageSize; }
    [[nodiscard]] std::size_t Pages() const { return _pages.size(); }
    [[nodiscard]] std::uint32_t UploadedThisFrame() const { return _uploaded; }

private:
    struct Entry {
        Placed placed;
        std::size_t page = 0;
    };
    struct Page {
        GpuTexture texture;
        AtlasPacker packer;
        std::uint64_t lastUsed = 0;
        std::vector<std::uint64_t> sprites;
    };
    struct PaletteRow {
        std::uint64_t revision = 0;
        std::uint64_t lastUsed = 0;
    };
    [[nodiscard]] std::size_t PageWithRoom(int width, int height, PixelRect& place);

    GpuDevice& _device;
    int _pageSize;
    std::size_t _pageBudget;
    std::vector<Page> _pages;
    std::unordered_map<std::uint64_t, Entry> _sprites;
    GpuTexture _palettes{};
    std::vector<PaletteRow> _rows;
    std::unordered_map<std::uint64_t, std::size_t> _rowOf;
    std::uint64_t _frame = 1;
    std::uint32_t _uploaded = 0;

public:
    PixelAtlas(const PixelAtlas&) = delete;
    PixelAtlas& operator=(const PixelAtlas&) = delete;
};
