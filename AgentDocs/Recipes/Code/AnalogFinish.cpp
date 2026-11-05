// [recipe:AnalogFinish]
#include "AnalogFinish.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"

namespace {
// Keep the calm finish light: it sits over the HUD for the whole game.
constexpr PostSettings Calm() {
    PostSettings post;
    post.vignette = 0.25f;
    post.grain.amount = 0.06f;
    return post;
}

// Dark and cold, with the moving effects only as strong as the HUD can take.
constexpr PostSettings Disturbed() {
    PostSettings post;
    post.vignette = 0.4f;
    post.aberration = 1.5f;
    post.fade = 0.6f;
    post.tint = {.color = Rgb(0x8c9ccc), .amount = 0.8f};
    post.grain.amount = 0.3f;
    post.wobble = {.amount = 6.0f, .speed = 1.8f};
    post.tape = {.band = 0.7f, .shift = 2.0f, .speed = 1.5f};
    return post;
}
} // namespace

void RecipeAnalogFinish::Apply(Painter& paint) const {
    paint.Post() = Mix(Calm(), Disturbed(), std::max(_danger, static_cast<float>(_shock)));
}
// [/recipe]
