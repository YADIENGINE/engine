#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/Patterns.h"

namespace {
GraphicAsset MakePatterns() {
    GraphicAsset asset;
    // In PatternKind's order.
    asset.items = {{"Dots", "background pattern polka grid tile"},
                   {"DotWave", "background pattern halftone ripple rings"},
                   {"Hexagons", "background pattern honeycomb hex grid tile"},
                   {"HexPulse", "background pattern honeycomb hex ripple rings"}};
    asset.usage = "DrawPattern(paint, box, PatternKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawPattern(paint, box, static_cast<PatternKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakePatterns);
} // namespace
