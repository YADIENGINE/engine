#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/StarField.h"

namespace {
GraphicAsset MakeStarField() {
    GraphicAsset asset;
    // In StarFieldKind's order.
    asset.items = {{"Still", "background space stars night twinkle"},
                   {"Drift", "background space stars parallax scrolling"},
                   {"Warp", "background space stars hyperspace speed streaks tunnel"}};
    asset.usage = "DrawStarField(paint, box, StarFieldKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawStarField(paint, box, static_cast<StarFieldKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeStarField);
} // namespace
