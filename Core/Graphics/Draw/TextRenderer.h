#pragma once
#include <raylib.h>
#include <filesystem>
#include <map>
#include <span>
#include <string_view>
#include <vector>
#include "Graphics/Draw/TextLayout.h"
#include "Graphics/Draw/TextStyle.h"

struct Rect;

// Owns the shared Unicode font after the window/OpenGL context is available. Text is rasterized for the pixel size it
// covers on the render target (font size x canvas and camera scale): sizes below the smallest preloaded atlas get an
// exact-size atlas on first use and are drawn texel-for-pixel on the pixel grid; larger text uses the smallest
// preloaded atlas at least that size. Measurements use the largest atlas' advances, so layout never depends on the
// window size.
class TextRenderer {
public:
    TextRenderer(const std::filesystem::path& file, std::span<const int> atlasSizes, std::span<const int> codepoints,
                 TextureFilter filter = TEXTURE_FILTER_BILINEAR);
    ~TextRenderer();

    void Draw(const char* text, int x, int y, int fontSize, Color color) const;
    [[nodiscard]] int Measure(const char* text, int fontSize) const;
    [[nodiscard]] int FittedSize(const char* text, int fontSize, int maxWidth) const;
    void DrawCentered(const char* text, int centerX, int y, int fontSize, Color color, int maxWidth = 0) const;
    void DrawAnchored(const char* text, Vector2 position, Color color, const TextStyle& style) const;
    [[nodiscard]] TextLayout Layout(std::string_view text, const TextLayoutOptions& options) const;
    void DrawMultiline(const TextLayout& layout, Rect bounds, Color color, const MultilineTextStyle& style = {}) const;

private:
    [[nodiscard]] const Font& AtlasFor(int pixelSize) const;

    std::vector<unsigned char> _fileData; // Tam boy atlaslar istendikçe buradan rasterize edilir
    std::vector<int> _codepoints;
    std::vector<Font> _fonts;           // Önceden yüklenen atlaslar, baseSize'a göre artan; sonuncusu ölçü kaynağı
    mutable std::map<int, Font> _exact; // En küçük atlastan küçük piksel boyları, ilk kullanımda
    bool _ownsFont = false;

public:
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    TextRenderer(TextRenderer&&) = delete;
    TextRenderer& operator=(TextRenderer&&) = delete;
};
