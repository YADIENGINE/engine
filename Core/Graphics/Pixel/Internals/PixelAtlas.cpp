#include "Graphics/Pixel/Internals/PixelAtlas.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"

namespace {
constexpr int SpritePadding = 1; // Nearest sampling needs no more
} // namespace

PixelAtlas::PixelAtlas(GpuDevice& device, int pageSize, std::size_t pageBudget, int paletteRows)
    : _device(device)
    , _pageSize(pageSize)
    , _pageBudget(std::max<std::size_t>(pageBudget, 1))
    , _rows(static_cast<std::size_t>(std::max(paletteRows, 1))) {}

PixelAtlas::~PixelAtlas() {
    for (const Page& page : _pages)
        _device.DestroyTexture(page.texture);
    if (_palettes.Valid()) _device.DestroyTexture(_palettes);
}

void PixelAtlas::BeginFrame() {
    ++_frame;
    _uploaded = 0;
}

std::size_t PixelAtlas::PageWithRoom(int width, int height, PixelRect& place) {
    for (std::size_t i = 0; i < _pages.size(); ++i) {
        if (const auto found = _pages[i].packer.Insert(width, height)) {
            place = *found;
            return i;
        }
    }
    if (_pages.size() >= _pageBudget) {
        std::size_t oldest = _pages.size();
        for (std::size_t i = 0; i < _pages.size(); ++i)
            if (_pages[i].lastUsed < _frame &&
                (oldest == _pages.size() || _pages[i].lastUsed < _pages[oldest].lastUsed))
                oldest = i;
        if (oldest < _pages.size()) {
            Page& page = _pages[oldest];
            for (const std::uint64_t id : page.sprites)
                _sprites.erase(id);
            page.sprites.clear();
            page.packer.Clear();
            if (const auto found = page.packer.Insert(width, height)) {
                place = *found;
                return oldest;
            }
        }
    }
    const GpuTexture texture =
        _device.CreateTexture({_pageSize, _pageSize, GpuFormat::R8, GpuFilter::Nearest, "Pixel sprite atlas"});
    _pages.push_back({texture, AtlasPacker(_pageSize, _pageSize, SpritePadding), _frame, {}});
    const auto found = _pages.back().packer.Insert(width, height);
    if (!found) throw std::logic_error("PixelAtlas: a sprite larger than an atlas page");
    place = *found;
    return _pages.size() - 1;
}

const PixelAtlas::Placed& PixelAtlas::Sprite(const PixelSprite& sprite) {
    if (sprite.Empty()) throw std::logic_error("PixelAtlas: an empty sprite has no place");
    if (const auto found = _sprites.find(sprite.Id()); found != _sprites.end()) {
        _pages[found->second.page].lastUsed = _frame;
        return found->second.placed;
    }
    PixelRect place;
    Entry entry;
    entry.page = PageWithRoom(sprite.Width(), sprite.Height(), place);
    Page& page = _pages[entry.page];
    _device.UpdateTexture(page.texture, place, std::as_bytes(sprite.Inks()));
    const auto size = static_cast<float>(_pageSize);
    entry.placed = {page.texture,
                    {static_cast<float>(place.x) / size, static_cast<float>(place.y) / size,
                     static_cast<float>(place.width) / size, static_cast<float>(place.height) / size}};
    page.lastUsed = _frame;
    page.sprites.push_back(sprite.Id());
    ++_uploaded;
    return _sprites.insert_or_assign(sprite.Id(), entry).first->second.placed;
}

PixelAtlas::Row PixelAtlas::Palette(const PixelPalette& palette) {
    const auto rows = static_cast<int>(_rows.size());
    if (!_palettes.Valid())
        _palettes = _device.CreateTexture({PaletteWidth, rows, GpuFormat::Rgba8, GpuFilter::Nearest, "Pixel palettes"});
    const auto centre = [&](std::size_t row) { return (static_cast<float>(row) + 0.5f) / static_cast<float>(rows); };
    if (const auto found = _rowOf.find(palette.Revision()); found != _rowOf.end()) {
        _rows[found->second].lastUsed = _frame;
        return {_palettes, centre(found->second)};
    }
    std::size_t row = _rows.size();
    for (std::size_t i = 0; i < _rows.size(); ++i)
        if (_rows[i].lastUsed < _frame && (row == _rows.size() || _rows[i].lastUsed < _rows[row].lastUsed)) row = i;
    if (row == _rows.size())
        throw std::runtime_error("PixelAtlas: more palettes in one frame than the palette texture has rows");
    if (_rows[row].revision != 0) _rowOf.erase(_rows[row].revision);
    std::array<std::uint8_t, PaletteWidth * 4> texels{};
    const auto colors = palette.Colors();
    for (std::size_t ink = 0; ink < colors.size() && ink < static_cast<std::size_t>(PaletteWidth); ++ink) {
        const Color4 color = colors[ink];
        const auto premultiplied = [&](std::uint8_t channel) {
            return static_cast<std::uint8_t>((static_cast<unsigned>(channel) * color.a + 127U) / 255U);
        };
        texels[ink * 4 + 0] = premultiplied(color.r);
        texels[ink * 4 + 1] = premultiplied(color.g);
        texels[ink * 4 + 2] = premultiplied(color.b);
        texels[ink * 4 + 3] = color.a;
    }
    _device.UpdateTexture(_palettes, {0, static_cast<int>(row), PaletteWidth, 1}, std::as_bytes(std::span(texels)));
    _rows[row] = {palette.Revision(), _frame};
    _rowOf[palette.Revision()] = row;
    ++_uploaded;
    return {_palettes, centre(row)};
}
