# Analog finish recipe

A worn-film look that grows with one number: film grain, a dark and cold colour, a bending image and a damaged tape, all from how near the danger is, plus a short shock that fades by itself.

APIs: Core/Graphics/Post/PostSettings.h, Core/Graphics/Post/RenderQuality.h, Core/Animation/Tween.h. Concepts: [Analog finish](../../Docs/Graphics/Painter.md#analog-finish).

The finish is view code. Set the number from what the simulation already knows; nothing here enters `State`, and a replay is the same with or without it. Don't check `RenderQuality::screenMotion` yourself: the renderer applies it.

<!-- recipe:AnalogFinish-header -->
```cpp
#pragma once
#include "Animation/Tween.h"
#include "Graphics/Post/PostSettings.h"

class Painter;

// A worn-film look that grows with one number: how near the danger is, or a shock that fades by itself. It is view code
// on seconds and decides nothing; the game sets the number from what its simulation already knows.
class RecipeAnalogFinish {
public:
    // Each frame, 0: nothing near, 1: right here.
    void SetDanger(float danger) { _danger = danger; }
    // A short scare on top of the danger, gone by itself in a second and a half.
    void Shock() { _shock.To(1.0f, 0.08f, Curve::OutCubic).Then(0.0f, 1.4f, Curve::InOutCubic); }
    void Update(float deltaSeconds) { _shock.Update(deltaSeconds); }
    // At the start of a frame's drawing.
    void Apply(Painter& paint) const;

private:
    float _danger = 0.0f;
    SecondsTween<float> _shock{0.0f};
};
```
<!-- /recipe -->

<!-- recipe:AnalogFinish -->
```cpp
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
```
<!-- /recipe -->

To tell it apart from `Fx`: `FxStage::Glitch` is a burst that ends by itself and is drawn on the canvas. This finish is a lasting value you keep setting. Look at both ends and at the HUD in the editor scene `engine.post.gallery`.
