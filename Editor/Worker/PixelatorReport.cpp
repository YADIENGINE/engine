#include "Editor/Worker/PixelatorReport.h"
#include <algorithm>
#include <exception>
#include "Editor/Internals/Pixelator/PixelatorFrames.h"
#include "Editor/Internals/Session/GraphicAssetCatalog.h"

PixelatorAsset PixelatorReport(const std::string& id) {
    PixelatorAsset report;
    report.id = id;
    try {
        const auto graphics = RegisteredGraphicThumbnails();
        const auto found = std::ranges::find(graphics, id, &GraphicThumbnail::id);
        if (found == graphics.end()) {
            report.error = "No registered graphic is called " + id;
            return report;
        }
        const auto asset = found->asset;
        if (!asset->pixels) {
            report.error = id + " is not a pixel graphic (its GraphicAsset has no pixels)";
            return report;
        }
        if (!asset->items.empty())
            for (const auto& graphic : graphics)
                if (graphic.asset == asset) {
                    if (graphic.id == id) report.item = report.items.size();
                    report.items.push_back({asset->items[graphic.item].name, graphic.id});
                }
        const std::size_t item = found->item;
        auto declared = PixelatorFramesOf(asset->pixels(item));
        declared.id = report.id;
        declared.items = std::move(report.items);
        declared.item = report.item;
        return declared;
    } catch (const std::exception& error) {
        report.items.clear();
        report.item = 0;
        report.error = std::string("Reading the pixels failed: ") + error.what();
        return report;
    }
}
