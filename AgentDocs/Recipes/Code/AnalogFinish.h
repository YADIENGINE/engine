// [recipe:AnalogFinish-header]
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
// [/recipe]
