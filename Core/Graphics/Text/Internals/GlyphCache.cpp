#include "Graphics/Text/Internals/GlyphCache.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Text/Internals/FontData.h"

namespace {
constexpr int GlyphPadding = 2; // Free pixels between glyphs (bilinear filtering)
constexpr int MinSize = 4;
constexpr int ExactLimit = 48;
constexpr int MaxSize = 192;
constexpr float Step = 1.06f;
} // namespace

std::size_t GlyphCache::KeyHash::operator()(const Key& key) const noexcept {
    std::size_t hash = key.font;
    hash = hash * 1000003u ^ static_cast<std::size_t>(key.glyph);
    hash = hash * 1000003u ^ static_cast<std::size_t>(key.size);
    return hash;
}

GlyphCache::GlyphCache(GpuDevice& device, int pageSize, std::size_t pageBudget)
    : _device(device)
    , _pageSize(pageSize)
    , _pageBudget(std::max<std::size_t>(pageBudget, 1)) {}

GlyphCache::~GlyphCache() {
    for (const Page& page : _pages)
        _device.DestroyTexture(page.texture);
}

void GlyphCache::BeginFrame() {
    ++_frame;
    _rasterized = 0;
}

int GlyphCache::RasterSize(float pixels) {
    if (!(pixels > 0.0f)) return MinSize;
    if (pixels <= static_cast<float>(ExactLimit)) return std::max(MinSize, static_cast<int>(std::lround(pixels)));
    const float steps = std::round(std::log(pixels / static_cast<float>(ExactLimit)) / std::log(Step));
    const auto size = static_cast<int>(std::lround(static_cast<float>(ExactLimit) * std::pow(Step, steps)));
    return std::clamp(size, ExactLimit, MaxSize);
}

const CachedGlyph& GlyphCache::Coverage(const FontData& font, int glyph, int pixelSize) {
    const Key key{font.Id(), glyph, std::max(pixelSize, 1)};
    if (const auto found = _entries.find(key); found != _entries.end()) {
        _pages[found->second.page].lastUsed = _frame;
        return found->second.glyph;
    }
    return Insert(key, font.Rasterize(glyph, static_cast<float>(key.size)));
}

const CachedGlyph& GlyphCache::Distance(const FontData& font, int glyph) {
    const Key key{font.Id(), glyph, 0};
    if (const auto found = _entries.find(key); found != _entries.end()) {
        _pages[found->second.page].lastUsed = _frame;
        return found->second.glyph;
    }
    return Insert(key, font.RasterizeDistance(glyph, static_cast<float>(DistanceSize), DistancePadding, DistanceScale));
}

std::size_t GlyphCache::PageWithRoom(int width, int height, PixelRect& place) {
    for (std::size_t i = 0; i < _pages.size(); ++i) {
        if (const auto found = _pages[i].packer.Insert(width, height)) {
            place = *found;
            return i;
        }
    }
    // Recycle the least recently used page that the current frame has not drawn from.
    if (_pages.size() >= _pageBudget) {
        std::size_t oldest = _pages.size();
        for (std::size_t i = 0; i < _pages.size(); ++i)
            if (_pages[i].lastUsed < _frame &&
                (oldest == _pages.size() || _pages[i].lastUsed < _pages[oldest].lastUsed))
                oldest = i;
        if (oldest < _pages.size()) {
            Page& page = _pages[oldest];
            for (const Key& key : page.keys)
                _entries.erase(key);
            page.keys.clear();
            page.packer.Clear();
            if (const auto found = page.packer.Insert(width, height)) {
                place = *found;
                return oldest;
            }
        }
    }
    const GpuTexture texture =
        _device.CreateTexture({_pageSize, _pageSize, GpuFormat::R8, GpuFilter::Linear, "Glyph atlas"});
    _pages.push_back({texture, AtlasPacker(_pageSize, _pageSize, GlyphPadding), _frame, {}});
    const auto found = _pages.back().packer.Insert(width, height);
    if (!found) throw std::runtime_error("GlyphCache: glyph larger than an atlas page");
    place = *found;
    return _pages.size() - 1;
}

const CachedGlyph& GlyphCache::Insert(const Key& key, const GlyphRaster& raster) {
    ++_rasterized;
    Entry entry;
    entry.glyph.left = static_cast<float>(raster.left);
    entry.glyph.top = static_cast<float>(raster.top);
    entry.glyph.width = static_cast<float>(raster.width);
    entry.glyph.height = static_cast<float>(raster.height);
    entry.glyph.empty = raster.width == 0 || raster.height == 0;
    if (!entry.glyph.empty) {
        PixelRect place;
        entry.page = PageWithRoom(raster.width, raster.height, place);
        Page& page = _pages[entry.page];
        _device.UpdateTexture(page.texture, place,
                              std::as_bytes(std::span<const std::uint8_t>(raster.pixels.data(), raster.pixels.size())));
        const auto size = static_cast<float>(_pageSize);
        entry.glyph.page = page.texture;
        entry.glyph.uv = {static_cast<float>(place.x) / size, static_cast<float>(place.y) / size,
                          static_cast<float>(place.width) / size, static_cast<float>(place.height) / size};
        page.lastUsed = _frame;
        page.keys.push_back(key);
    }
    return _entries.insert_or_assign(key, entry).first->second.glyph;
}
