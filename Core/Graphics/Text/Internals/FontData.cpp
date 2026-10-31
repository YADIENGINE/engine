#include "Graphics/Text/Internals/FontData.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>

// stb_truetype comes with raylib (unchanged, ThirdParty/raylib/src/external); this translation unit compiles a private
// copy (STBTT_STATIC: no exported symbols, so raylib's own static copy never collides).
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "stb_truetype.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace {
constexpr int AsciiCount = 128;

std::atomic<std::uint32_t>& NextFontId() {
    static std::atomic<std::uint32_t> next{1};
    return next;
}
} // namespace

struct FontData::Info {
    stbtt_fontinfo font{};
};

FontData::FontData(std::vector<unsigned char> bytes)
    : _bytes(std::move(bytes))
    , _info(std::make_unique<Info>())
    , _id(NextFontId().fetch_add(1)) {
    const int offset = _bytes.empty() ? -1 : stbtt_GetFontOffsetForIndex(_bytes.data(), 0);
    if (offset < 0 || stbtt_InitFont(&_info->font, _bytes.data(), offset) == 0)
        throw std::runtime_error("Not a usable TrueType / OpenType font");
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&_info->font, &ascent, &descent, &lineGap);
    if (ascent - descent <= 0) throw std::runtime_error("Font has no vertical metrics");
    _ascent = static_cast<float>(ascent);
    _descent = static_cast<float>(descent);
    _lineGap = static_cast<float>(lineGap);
    _fallback = stbtt_FindGlyphIndex(&_info->font, '?');
    _ascii.resize(AsciiCount);
    for (int codepoint = 0; codepoint < AsciiCount; ++codepoint) {
        const int glyph = stbtt_FindGlyphIndex(&_info->font, codepoint);
        _ascii[static_cast<std::size_t>(codepoint)] = glyph != 0 ? glyph : _fallback;
    }
}

FontData::~FontData() = default;

int FontData::GlyphIndex(char32_t codepoint) const {
    if (codepoint < AsciiCount) return _ascii[codepoint];
    const int glyph = stbtt_FindGlyphIndex(&_info->font, static_cast<int>(codepoint));
    return glyph != 0 ? glyph : _fallback;
}

bool FontData::Has(char32_t codepoint) const {
    return stbtt_FindGlyphIndex(&_info->font, static_cast<int>(codepoint)) != 0;
}

float FontData::Advance(int glyph) const {
    int advance = 0;
    int bearing = 0;
    stbtt_GetGlyphHMetrics(&_info->font, glyph, &advance, &bearing);
    return static_cast<float>(advance);
}

float FontData::Kerning(int left, int right) const {
    return static_cast<float>(stbtt_GetGlyphKernAdvance(&_info->font, left, right));
}

GlyphRaster FontData::Rasterize(int glyph, float pixelSize) const {
    GlyphRaster raster;
    const float scale = stbtt_ScaleForPixelHeight(&_info->font, pixelSize);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetGlyphBitmapBox(&_info->font, glyph, scale, scale, &x0, &y0, &x1, &y1);
    raster.width = std::max(0, x1 - x0);
    raster.height = std::max(0, y1 - y0);
    raster.left = x0;
    raster.top = y0;
    if (raster.width == 0 || raster.height == 0) return raster;
    raster.pixels.assign(static_cast<std::size_t>(raster.width) * static_cast<std::size_t>(raster.height), 0);
    stbtt_MakeGlyphBitmap(&_info->font, raster.pixels.data(), raster.width, raster.height, raster.width, scale, scale,
                          glyph);
    return raster;
}

GlyphRaster FontData::RasterizeDistance(int glyph, float pixelSize, int padding, float distanceScale) const {
    GlyphRaster raster;
    const float scale = stbtt_ScaleForPixelHeight(&_info->font, pixelSize);
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    unsigned char* field =
        stbtt_GetGlyphSDF(&_info->font, scale, glyph, padding, 128, distanceScale, &width, &height, &left, &top);
    if (!field) return raster;
    raster.width = width;
    raster.height = height;
    raster.left = left;
    raster.top = top;
    raster.pixels.assign(field, field + static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    stbtt_FreeSDF(field, nullptr);
    return raster;
}
