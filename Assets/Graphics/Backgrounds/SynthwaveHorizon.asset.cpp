#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Backgrounds/SynthwaveHorizon.h"

namespace {
GraphicAsset MakeSynthwaveHorizon() {
    GraphicAsset asset;
    // In SynthwaveKind's order.
    asset.items = {{"Magenta", "background retro neon outrun grid sun pink purple"},
                   {"Cyan", "background retro neon outrun grid sun blue"},
                   {"Ember", "background retro neon outrun grid sun orange red"}};
    asset.usage = "DrawSynthwaveHorizon(paint, box, SynthwaveKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawSynthwaveHorizon(paint, box, static_cast<SynthwaveKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeSynthwaveHorizon);
} // namespace
