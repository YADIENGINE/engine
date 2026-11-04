#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/Skyline.h"

namespace {
GraphicAsset MakeSkyline() {
    GraphicAsset asset;
    // In SkyKind's order.
    asset.items = {{"Day", "background city buildings silhouette urban towers parallax"},
                   {"Sunset", "background city buildings silhouette urban towers dusk parallax"},
                   {"Night", "background city buildings silhouette urban towers lights windows parallax"}};
    asset.usage = "DrawSkyline(paint, box, SkyKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawSkyline(paint, box, static_cast<SkyKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeSkyline);
} // namespace
