#include "Graphics/Draw/TextRenderer.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include "Graphics/Render/Internals/RenderTargetStack.h"
#include "Runtime/Internals/StartupTimer.h"
#include "Utilities/Rect.h"

namespace {
constexpr int DefaultGlyphCount = 95; // Kod noktası verilmezse ASCII 32..126
constexpr int MinExactSize = 4;       // Daha küçük piksel boyu bu boyda rasterize edilir
constexpr float LineGap = 2.0f;       // '\n' sonrası satır arası (raylib'in DrawTextEx varsayılanı)

// Atlasta glifler arası boşluk (raylib'in LoadFontEx'i 4 kullanır). Mipmap'li küçültmede (trilinear) her seviye boşluğu
// yarıya indirir; geniş boşluk küçük yazılarda komşu glifin sızmasını önler.
int GlyphPadding(TextureFilter filter) { return filter == TEXTURE_FILTER_TRILINEAR ? 16 : 4; }

// Glifleri tek boyutta rasterize eder; glyphs null dönerse yükleme başarısızdır (atlas kurulmaz).
Font LoadAtlas(std::span<const unsigned char> data, int size, std::span<const int> codepoints, TextureFilter filter) {
    std::vector<int> requested(codepoints.begin(), codepoints.end()); // LoadFontData sabit olmayan dizi ister
    Font font{};
    font.baseSize = size;
    font.glyphCount = requested.empty() ? DefaultGlyphCount : static_cast<int>(requested.size());
    font.glyphPadding = GlyphPadding(filter);
    font.glyphs = LoadFontData(data.data(), static_cast<int>(data.size()), size,
                               requested.empty() ? nullptr : requested.data(), font.glyphCount, FONT_DEFAULT);
    if (font.glyphs == nullptr) return font;
    const Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, size, font.glyphPadding, 0);
    font.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    if (filter == TEXTURE_FILTER_TRILINEAR) GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, filter);
    return font;
}

// Glifin kalem ilerlemesi, atlasın kendi piksel biriminde (raylib'in DrawTextEx kuralı).
float Advance(const Font& font, int codepoint) {
    const int glyph = GetGlyphIndex(font, codepoint);
    const int advance = font.glyphs[glyph].advanceX;
    return static_cast<float>(advance != 0 ? advance : static_cast<int>(font.recs[glyph].width));
}
} // namespace

TextRenderer::TextRenderer(const std::filesystem::path& file, std::span<const int> atlasSizes,
                           std::span<const int> codepoints, TextureFilter filter)
    : _codepoints(codepoints.begin(), codepoints.end()) {
    if (file.empty()) {
        _fonts.push_back(GetFontDefault());
        return;
    }
    if (atlasSizes.empty()) throw std::invalid_argument("Font needs at least one atlas size");
    const std::string path = file.string();
    int dataSize = 0;
    unsigned char* data = LoadFileData(path.c_str(), &dataSize);
    if (data == nullptr) throw std::runtime_error("Could not load font: " + path);
    _fileData.assign(data, data + dataSize);
    UnloadFileData(data);
    std::vector<int> sizes(atlasSizes.begin(), atlasSizes.end());
    std::ranges::sort(sizes);
    const StartupTimer timer("Font atlases (all)");
    try {
        for (const int size : sizes) {
            const Font font = LoadAtlas(_fileData, size, _codepoints, filter);
            if (font.glyphs == nullptr) throw std::runtime_error("Could not load font: " + path);
            _fonts.push_back(font);
            if (!IsFontValid(font)) throw std::runtime_error("Could not load font: " + path);
        }
    } catch (...) {
        for (const Font& font : _fonts)
            UnloadFont(font);
        throw;
    }
    _ownsFont = true;
}
TextRenderer::~TextRenderer() {
    if (!_ownsFont) return;
    for (const Font& font : _fonts)
        UnloadFont(font);
    for (const auto& [size, font] : _exact)
        UnloadFont(font);
}

const Font& TextRenderer::AtlasFor(int pixelSize) const {
    if (_fileData.empty() || pixelSize >= _fonts.front().baseSize) {
        const auto atlas =
            std::ranges::find_if(_fonts, [pixelSize](const Font& font) { return font.baseSize >= pixelSize; });
        return atlas == _fonts.end() ? _fonts.back() : *atlas;
    }
    const int size = std::max(pixelSize, MinExactSize);
    if (const auto cached = _exact.find(size); cached != _exact.end()) return cached->second;
    // Ekrana texel başına bir piksel düşer; mipmap gerekmez.
    const Font font = LoadAtlas(_fileData, size, _codepoints, TEXTURE_FILTER_BILINEAR);
    if (font.glyphs == nullptr) return _fonts.front();
    if (!IsFontValid(font)) {
        UnloadFont(font);
        return _fonts.front();
    }
    return _exact.emplace(size, font).first->second;
}

void TextRenderer::Draw(const char* text, int x, int y, int fontSize, Color color) const {
    if (fontSize <= 0) return;
    const auto size = static_cast<float>(fontSize);
    const float pixelScale = PixelScale();
    const int pixelSize = static_cast<int>(std::lround(size * pixelScale));
    const Font& atlas = AtlasFor(pixelSize);
    const Font& metrics = _fonts.back();
    const float advanceScale = size / static_cast<float>(metrics.baseSize);
    // Boyu tutan atlas texel başına bir piksel çizilir (keskin); büyük atlas yazı boyuna küçültülür.
    const float glyphScale = atlas.baseSize == pixelSize && pixelScale > 0.0f
                                 ? 1.0f / pixelScale
                                 : size / static_cast<float>(atlas.baseSize);
    const auto padding = static_cast<float>(atlas.glyphPadding);

    Vector2 pen{static_cast<float>(x), static_cast<float>(y)};
    for (const char* cursor = text; *cursor != '\0';) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(cursor, &bytes);
        cursor += bytes;
        if (codepoint == '\n') {
            pen = {static_cast<float>(x), pen.y + size + LineGap};
            continue;
        }
        if (codepoint != ' ' && codepoint != '\t') {
            const int glyph = GetGlyphIndex(atlas, codepoint);
            const Rectangle& rec = atlas.recs[glyph];
            const Rectangle source{rec.x - padding, rec.y - padding, rec.width + 2.0f * padding,
                                   rec.height + 2.0f * padding};
            // Glifin köşesi piksel ızgarasına oturur: texel'ler piksellerle hizalanır, kenarlar bulanmaz.
            const Vector2 corner =
                AlignToPixel({pen.x + (static_cast<float>(atlas.glyphs[glyph].offsetX) - padding) * glyphScale,
                              pen.y + (static_cast<float>(atlas.glyphs[glyph].offsetY) - padding) * glyphScale});
            DrawTexturePro(atlas.texture, source,
                           {corner.x, corner.y, source.width * glyphScale, source.height * glyphScale}, {0.0f, 0.0f},
                           0.0f, color);
        }
        pen.x += Advance(metrics, codepoint) * advanceScale;
    }
}

int TextRenderer::Measure(const char* text, int fontSize) const {
    const Font& metrics = _fonts.back();
    float widest = 0.0f;
    float line = 0.0f;
    for (const char* cursor = text; *cursor != '\0';) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(cursor, &bytes);
        cursor += bytes;
        if (codepoint == '\n') {
            widest = std::max(widest, line);
            line = 0.0f;
            continue;
        }
        line += Advance(metrics, codepoint);
    }
    widest = std::max(widest, line);
    return static_cast<int>(widest * static_cast<float>(fontSize) / static_cast<float>(metrics.baseSize) + 0.5f);
}

int TextRenderer::FittedSize(const char* text, int fontSize, int maxWidth) const {
    return FittedTextSize(text, fontSize, maxWidth,
                          [this](const char* label, int size) { return Measure(label, size); });
}

void TextRenderer::DrawCentered(const char* text, int centerX, int y, int fontSize, Color color, int maxWidth) const {
    const int fittedSize = FittedSize(text, fontSize, maxWidth);
    Draw(text, centerX - Measure(text, fittedSize) / 2, y + (fontSize - fittedSize) / 2, fittedSize, color);
}

void TextRenderer::DrawAnchored(const char* text, Vector2 position, Color color, const TextStyle& style) const {
    if (style.fontSize <= 0) return;
    const int fitted = FittedSize(text, style.fontSize, style.maxWidth);
    const int x =
        static_cast<int>(position.x) - static_cast<int>(static_cast<float>(Measure(text, fitted)) * style.anchor.x);
    const int y = static_cast<int>(position.y) - static_cast<int>(static_cast<float>(style.fontSize) * style.anchor.y) +
                  (style.fontSize - fitted) / 2;
    Color shadow = style.shadow;
    shadow.a = static_cast<unsigned char>(static_cast<float>(shadow.a) * static_cast<float>(color.a) / 255.0f);
    if (shadow.a > 0) {
        Draw(text, x + static_cast<int>(style.shadowOffset.x), y + static_cast<int>(style.shadowOffset.y), fitted,
             shadow);
    }
    Draw(text, x, y, fitted, color);
}

TextLayout TextRenderer::Layout(std::string_view text, const TextLayoutOptions& options) const {
    return LayoutText(text, options, [this](const char* label, int size) { return Measure(label, size); });
}

void TextRenderer::DrawMultiline(const TextLayout& layout, Rect bounds, Color color,
                                 const MultilineTextStyle& style) const {
    if (style.lineSpacing < 0) throw std::invalid_argument("Multiline spacing must be nonnegative");
    int y = bounds.y;
    for (const auto& line : layout.lines) {
        const int x = style.alignment == TextAlignment::Center ? bounds.CenterX() - line.width / 2 : bounds.x;
        Draw(line.text.c_str(), x, y + (layout.nominalFontSize - line.fontSize) / 2, line.fontSize, color);
        y += layout.nominalFontSize + style.lineSpacing;
    }
}
