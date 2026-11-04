#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/Mountains.h"

namespace {
GraphicAsset MakeMountains() {
    GraphicAsset asset;
    // In SkyKind's order.
    asset.items = {{"Day", "background landscape ridges peaks snow pines parallax"},
                   {"Sunset", "background landscape ridges peaks dusk parallax"},
                   {"Night", "background landscape ridges peaks moon stars parallax"}};
    asset.usage = "DrawMountains(paint, box, SkyKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawMountains(paint, box, static_cast<SkyKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeMountains);
} // namespace
