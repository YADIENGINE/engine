#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Ui/Backdrop.h"

namespace {
GraphicAsset MakeBackdrops() {
    GraphicAsset asset;
    // In BackdropRole's order.
    asset.items = {{"Menu", "background grid"}, {"Play", "background grid"}, {"Gallery", "background grid"}};
    asset.usage = "DrawBackdropGrid(paint, BackdropRole::{item}, area);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float) {
        const auto role = static_cast<BackdropRole>(item);
        paint.Rect(box, Fill(CurrentTheme().palette.background));
        DrawBackdropGrid(paint, role, box);
        DrawBackdropVignette(paint, role, box);
        DrawBackdropScanlines(paint, role, box);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeBackdrops);
} // namespace
