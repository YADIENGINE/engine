#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/Sea.h"

namespace {
GraphicAsset MakeSea() {
    GraphicAsset asset;
    // In SkyKind's order.
    asset.items = {{"Day", "background ocean water waves horizon sun"},
                   {"Sunset", "background ocean water waves horizon dusk reflection"},
                   {"Night", "background ocean water waves horizon moon reflection"}};
    asset.usage = "DrawSea(paint, box, SkyKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawSea(paint, box, static_cast<SkyKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeSea);
} // namespace
