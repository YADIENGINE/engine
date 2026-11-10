#include "Editor/Internals/Pixelator/PixelatorPicture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include "Editor/Internals/Pixelator/PixelatorView.h"

namespace {
constexpr Color4 Surround{30, 32, 37, 255}; // Around the sprite in a composed picture
constexpr int Margin = 8;                   // Pixels around the sprite, so a pivot on its edge shows whole
constexpr int PivotArm = 6;
constexpr int PivotWidth = 2;

std::uint8_t Mix(std::uint8_t over, std::uint8_t under, int alpha) {
    return static_cast<std::uint8_t>((over * alpha + under * (255 - alpha) + 127) / 255);
}

class Canvas {
public:
    Canvas(int width, int height, Color4 fill)
        : _width(width)
        , _height(height) {
        _pixels.width = width;
        _pixels.height = height;
        _pixels.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                Set(x, y, fill);
    }
    void Set(int x, int y, Color4 color) {
        auto* pixel =
            &_pixels
                 .rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) + static_cast<std::size_t>(x)) *
                       4];
        pixel[0] = std::byte{color.r};
        pixel[1] = std::byte{color.g};
        pixel[2] = std::byte{color.b};
        pixel[3] = std::byte{255};
    }
    void Blend(int x, int y, Color4 color) {
        if (x < 0 || y < 0 || x >= _width || y >= _height || color.a == 0) return;
        auto* pixel =
            &_pixels
                 .rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) + static_cast<std::size_t>(x)) *
                       4];
        pixel[0] = std::byte{Mix(color.r, std::to_integer<std::uint8_t>(pixel[0]), color.a)};
        pixel[1] = std::byte{Mix(color.g, std::to_integer<std::uint8_t>(pixel[1]), color.a)};
        pixel[2] = std::byte{Mix(color.b, std::to_integer<std::uint8_t>(pixel[2]), color.a)};
    }
    [[nodiscard]] ViewportPixels Take() { return std::move(_pixels); }

private:
    int _width;
    int _height;
    ViewportPixels _pixels;
};
} // namespace

Color4 PixelatorIndexColor(int index) {
    const float hue = std::fmod(static_cast<float>(index) * 0.61803398875f, 1.0f) * 6.0f;
    const float x = 1.0f - std::abs(std::fmod(hue, 2.0f) - 1.0f);
    std::array<float, 3> rgb{};
    const int sector = static_cast<int>(hue);
    switch (sector) {
    case 0 : rgb = {1.0f, x, 0.0f}; break;
    case 1 : rgb = {x, 1.0f, 0.0f}; break;
    case 2 : rgb = {0.0f, 1.0f, x}; break;
    case 3 : rgb = {0.0f, x, 1.0f}; break;
    case 4 : rgb = {x, 0.0f, 1.0f}; break;
    default: rgb = {1.0f, 0.0f, x}; break;
    }
    const auto channel = [](float value) { return static_cast<std::uint8_t>(std::lround(60.0f + value * 195.0f)); };
    return {channel(rgb[0]), channel(rgb[1]), channel(rgb[2]), 255};
}

std::vector<PixelatorCount> PixelatorInkCounts(const PixelatorImage& image) {
    std::array<int, MaxInks> counts{};
    for (const Ink ink : image.inks)
        ++counts[ink];
    std::vector<PixelatorCount> result;
    for (int ink = 1; ink < MaxInks; ++ink)
        if (counts[static_cast<std::size_t>(ink)] > 0) result.push_back({ink, counts[static_cast<std::size_t>(ink)]});
    return result;
}

std::vector<PixelatorCount> PixelatorPartCounts(const PixelatorImage& image) {
    std::array<int, 256> counts{};
    for (std::size_t index = 0; index < image.inks.size(); ++index)
        if (image.inks[index] != ClearInk) ++counts[image.parts[index]];
    std::vector<PixelatorCount> result;
    for (int part = 0; part < 256; ++part)
        if (counts[static_cast<std::size_t>(part)] > 0)
            result.push_back({part, counts[static_cast<std::size_t>(part)]});
    return result;
}

std::vector<Color4> PixelatorColors(const PixelatorImage& image, std::span<const Color4> palette,
                                    PixelatorViewMode mode, std::optional<Ink> focus) {
    std::vector<Color4> colors(image.inks.size(), Color4{0, 0, 0, 0}); // Ink 0 is transparent
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const Ink ink = image.inks[index];
        if (ink == ClearInk) continue;
        Color4 color;
        switch (mode) {
        case PixelatorViewMode::Color: color = ink < palette.size() ? palette[ink] : Color4{0, 0, 0, 0}; break;
        case PixelatorViewMode::Inks : color = PixelatorIndexColor(ink); break;
        case PixelatorViewMode::Parts: color = PixelatorIndexColor(image.parts[index] + 1); break;
        }
        if (focus && ink != *focus) color.a = static_cast<std::uint8_t>(color.a * PixelatorDimAlpha / 255);
        colors[index] = color;
    }
    return colors;
}

Color4 PixelatorBackgroundColor(PixelatorBackground background, bool alternate) {
    switch (background) {
    case PixelatorBackground::Checker: return alternate ? PixelatorCheckerDark : PixelatorCheckerLight;
    case PixelatorBackground::Dark   : return PixelatorDarkBackground;
    case PixelatorBackground::Light  : return PixelatorLightBackground;
    }
    return PixelatorDarkBackground;
}

int PixelatorCheckerSize(int zoom) {
    zoom = std::max(1, zoom);
    return zoom * ((PixelatorCheckerCell + zoom - 1) / zoom);
}

std::vector<int> PixelatorTileLines(int length, int tile) {
    std::vector<int> lines;
    if (tile <= 0) return lines;
    for (int at = tile; at < length; at += tile)
        lines.push_back(at);
    return lines;
}

int PixelatorPictureZoom(Vec2 sprite, int requested) {
    const int side = std::max({sprite.x, sprite.y, 1});
    int zoom = requested > 0
                   ? std::clamp(requested, PixelatorView::MinZoom, PixelatorView::MaxZoom)
                   : std::clamp(PixelatorPictureAutoSide / side, PixelatorView::MinZoom, PixelatorView::MaxZoom);
    while (zoom > 1 && side * zoom + Margin * 2 > PixelatorPictureMaxSide)
        --zoom;
    return zoom;
}

ViewportPixels ComposePixelatorPicture(const PixelatorImage& image, std::span<const Color4> palette,
                                       const PixelatorPictureOptions& options, const PixelatorPictureExtras& extras) {
    const int zoom = options.zoom;
    if (image.Empty()) throw std::invalid_argument("The frame has no pixels");
    if (zoom < PixelatorView::MinZoom || zoom > PixelatorView::MaxZoom)
        throw std::invalid_argument("Pixelator zoom must be from 1 to 64");
    const int repeat = std::clamp(extras.repeat, 1, 8);
    const int width = image.width * zoom * repeat;
    const int height = image.height * zoom * repeat;
    if (width + Margin * 2 > PixelatorPictureMaxSide || height + Margin * 2 > PixelatorPictureMaxSide)
        throw std::invalid_argument("The picture would be too large; use a smaller zoom");
    Canvas canvas(width + Margin * 2, height + Margin * 2, Surround);
    const int checker = PixelatorCheckerSize(zoom);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            canvas.Set(Margin + x, Margin + y,
                       PixelatorBackgroundColor(options.background, (x / checker + y / checker) % 2 == 1));
    const auto paint = [&](const PixelatorImage& layer, const std::vector<Color4>& colors, Vec2 offset, int copyX,
                           int copyY) {
        const int baseX = Margin + copyX * image.width * zoom;
        const int baseY = Margin + copyY * image.height * zoom;
        for (int py = 0; py < layer.height; ++py)
            for (int px = 0; px < layer.width; ++px) {
                const Color4 color = colors[static_cast<std::size_t>(py * layer.width + px)];
                const int sx = px + offset.x;
                const int sy = py + offset.y;
                if (color.a == 0 || sx < 0 || sy < 0 || sx >= image.width || sy >= image.height) continue;
                for (int y = 0; y < zoom; ++y)
                    for (int x = 0; x < zoom; ++x)
                        canvas.Blend(baseX + sx * zoom + x, baseY + sy * zoom + y, color);
            }
    };
    const auto colors = extras.colors ? *extras.colors : PixelatorColors(image, palette, options.view, options.focus);
    for (int copyY = 0; copyY < repeat; ++copyY)
        for (int copyX = 0; copyX < repeat; ++copyX) {
            for (const auto& layer : extras.under)
                if (layer.image && layer.colors.size() == layer.image->inks.size())
                    paint(*layer.image, layer.colors, layer.offset, copyX, copyY);
            paint(image, colors, {}, copyX, copyY);
        }
    const auto column = [&](int x, Color4 color) {
        for (int y = 0; y < height; ++y)
            canvas.Blend(Margin + x, Margin + y, color);
    };
    const auto row = [&](int y, Color4 color) {
        for (int x = 0; x < width; ++x)
            canvas.Blend(Margin + x, Margin + y, color);
    };
    const int columns = image.width * repeat;
    const int rows = image.height * repeat;
    if (options.grid.value_or(zoom >= PixelatorView::GridFromZoom)) {
        for (int x = 1; x < columns; ++x)
            column(x * zoom, PixelatorGridColor);
        for (int y = 1; y < rows; ++y)
            row(y * zoom, PixelatorGridColor);
    }
    const int tiles = repeat > 1 && options.tiles == 0 ? 0 : options.tiles;
    for (const int x : PixelatorTileLines(columns, tiles)) {
        column(x * zoom, PixelatorTileColor);
        if (zoom >= 8) column(x * zoom - 1, PixelatorTileColor);
    }
    for (const int y : PixelatorTileLines(rows, tiles)) {
        row(y * zoom, PixelatorTileColor);
        if (zoom >= 8) row(y * zoom - 1, PixelatorTileColor);
    }
    for (const Vec2 mark : extras.marks) {
        if (mark.x < 0 || mark.y < 0 || mark.x >= image.width || mark.y >= image.height) continue;
        const int left = Margin + mark.x * zoom - 1;
        const int top = Margin + mark.y * zoom - 1;
        const int side = zoom + 2;
        for (int step = 0; step < side; ++step) {
            canvas.Blend(left + step, top, PixelatorMarkColor);
            canvas.Blend(left + step, top + side - 1, PixelatorMarkColor);
            canvas.Blend(left, top + step, PixelatorMarkColor);
            canvas.Blend(left + side - 1, top + step, PixelatorMarkColor);
        }
    }
    if (options.pivot && repeat == 1) {
        const int cx = Margin + image.pivot.x * zoom;
        const int cy = Margin + image.pivot.y * zoom;
        constexpr int First = -PivotWidth / 2;
        constexpr int Last = PivotWidth - PivotWidth / 2; // Exclusive
        for (int offset = -PivotArm; offset < PivotArm; ++offset)
            for (int thick = First; thick < Last; ++thick) {
                canvas.Blend(cx + offset, cy + thick, PixelatorPivotColor);
                if (offset < First || offset >= Last) canvas.Blend(cx + thick, cy + offset, PixelatorPivotColor);
            }
    }
    return canvas.Take();
}

ViewportPixels PadPixelatorPicture(const ViewportPixels& picture, int width, int height) {
    width = std::max(width, picture.width);
    height = std::max(height, picture.height);
    Canvas canvas(width, height, Surround);
    auto padded = canvas.Take();
    const auto rowBytes = static_cast<std::ptrdiff_t>(picture.width) * 4;
    for (int y = 0; y < picture.height; ++y)
        std::copy_n(picture.rgba.begin() + y * rowBytes, rowBytes,
                    padded.rgba.begin() + static_cast<std::ptrdiff_t>(y) * width * 4);
    return padded;
}
