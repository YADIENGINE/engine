#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/Clouds.h"

namespace {
GraphicAsset MakeClouds() {
    GraphicAsset asset;
    // In SkyKind's order.
    asset.items = {{"Day", "background sky cumulus puffy white weather"},
                   {"Sunset", "background sky cumulus puffy dusk weather"},
                   {"Night", "background sky cumulus puffy moonlit weather"}};
    asset.usage = "DrawClouds(paint, box, SkyKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawClouds(paint, box, static_cast<SkyKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeClouds);
} // namespace
