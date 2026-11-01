#include "Editor/Worker/AssetThumbnailGame.h"
#include <raylib.h>
#include <algorithm>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include "Graphics/Paint/Painter.h"

namespace {
constexpr std::size_t RowsPerPage = 3;
constexpr float CellMargin = 0.1f; // Of the cell, so glows have room before the clip
} // namespace

AssetThumbnailGame::AssetThumbnailGame(const GameSettings& settings, std::filesystem::path directory)
    : Game(settings)
    , _directory(std::move(directory))
    , _thumbnails(RegisteredGraphicThumbnails())
    , _canvas{static_cast<float>(settings.canvasWidth), static_cast<float>(settings.canvasHeight)} {
    _cellSide = _canvas.y / static_cast<float>(RowsPerPage);
    _columns = std::max<std::size_t>(1, static_cast<std::size_t>(_canvas.x / _cellSide));
    _rows = RowsPerPage;
    std::filesystem::create_directories(_directory);
}

FloatRect AssetThumbnailGame::Cell(std::size_t index) const {
    return {static_cast<float>(index % _columns) * _cellSide, static_cast<float>(index / _columns) * _cellSide,
            _cellSide, _cellSide};
}

void AssetThumbnailGame::Update(float realSeconds) {
    (void)realSeconds;
    if (_next >= _thumbnails.size()) RequestQuit();
}

void AssetThumbnailGame::Draw(Painter& paint) const {
    const auto count = std::min(_columns * _rows, _thumbnails.size() - std::min(_next, _thumbnails.size()));
    for (std::size_t index = 0; index < count; ++index) {
        const auto& thumbnail = _thumbnails[_next + index];
        const auto cell = Cell(index);
        const float inset = cell.width * CellMargin;
        const FloatRect box{cell.x + inset, cell.y + inset, cell.width - inset * 2, cell.height - inset * 2};
        const auto clip = paint.Clip(cell);
        try {
            thumbnail.asset->draw(paint, box, thumbnail.item, thumbnail.asset->thumbnailTime);
        } catch (const std::exception& error) { // One bad drawing must not cost the others their thumbnails
            std::fprintf(stderr, "Thumbnail of %s: %s\n", thumbnail.id.c_str(), error.what());
        }
    }
    _drawn = true;
}

void AssetThumbnailGame::CanvasReady(const Texture2D& texture) const {
    if (!_drawn || _next >= _thumbnails.size()) return;
    _drawn = false;
    ::Image page = LoadImageFromTexture(texture);
    if (!page.data) throw std::runtime_error("Thumbnail readback failed");
    ImageFormat(&page, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageFlipVertical(&page);
    const float scale = static_cast<float>(texture.width) / _canvas.x;
    const auto count = std::min(_columns * _rows, _thumbnails.size() - _next);
    for (std::size_t index = 0; index < count; ++index) {
        const auto cell = Cell(index);
        ::Image image = ImageFromImage(page, {cell.x * scale, cell.y * scale, cell.width * scale, cell.height * scale});
        const auto file = _directory / AssetThumbnailFile(_thumbnails[_next + index].id);
        const bool written = ExportImage(image, file.string().c_str());
        UnloadImage(image);
        if (!written) std::fprintf(stderr, "Could not write %s\n", file.string().c_str());
    }
    UnloadImage(page);
    _next += count;
}
