#pragma once
#include <cstddef>
#include <filesystem>
#include <vector>
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Runtime/Game.h"

// Draws every registered graphic's thumbnail, page by page on the hidden canvas, writes them as PNG files and quits.
// Its own short-lived process, so a running scene is never disturbed and old files stay usable during a broken build.
class AssetThumbnailGame final : public Game {
public:
    AssetThumbnailGame(const GameSettings& settings, std::filesystem::path directory);

protected:
    void Update(float realSeconds) override;
    void Draw(Painter& paint) const override;
    void CanvasReady(const Texture2D& texture) const override;

private:
    [[nodiscard]] FloatRect Cell(std::size_t index) const;
    std::filesystem::path _directory;
    std::vector<GraphicThumbnail> _thumbnails;
    Float2 _canvas;
    float _cellSide = 0;
    std::size_t _columns = 1;
    std::size_t _rows = 1;
    mutable std::size_t _next = 0; // First thumbnail of the page being drawn
    mutable bool _drawn = false;
};
