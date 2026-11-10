#include "Editor/Worker/PixelatorReport.h"
#include <algorithm>
#include <exception>
#include "Editor/Internals/Pixelator/PixelFrameSampling.h"
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
        auto sampled = SamplePixelFrames([&](float time) { return asset->pixels(item, time); });
        sampled.id = report.id;
        sampled.items = std::move(report.items);
        sampled.item = report.item;
        return sampled;
    } catch (const std::exception& error) {
        report.items.clear();
        report.item = 0;
        report.error = std::string("Sampling the pixels failed: ") + error.what();
        return report;
    }
}
