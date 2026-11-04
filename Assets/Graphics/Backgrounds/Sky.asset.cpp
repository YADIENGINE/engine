#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/Sky.h"

namespace {
GraphicAsset MakeSky() {
    GraphicAsset asset;
    // In SkyKind's order.
    asset.items = {{"Day", "background sky sun blue daytime gradient"},
                   {"Sunset", "background sky sun dusk evening orange gradient"},
                   {"Night", "background sky moon stars dark gradient"}};
    asset.usage = "DrawSky(paint, box, SkyKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawSky(paint, box, static_cast<SkyKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeSky);
} // namespace
