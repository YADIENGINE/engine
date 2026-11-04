#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/Nebula.h"

namespace {
GraphicAsset MakeNebula() {
    GraphicAsset asset;
    // In NebulaKind's order.
    asset.items = {{"Violet", "background space gas cloud purple pink stars"},
                   {"Teal", "background space gas cloud green blue stars"},
                   {"Ember", "background space gas cloud red orange stars"}};
    asset.usage = "DrawNebula(paint, box, NebulaKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawNebula(paint, box, static_cast<NebulaKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeNebula);
} // namespace
