// [recipe:AnimatedCharacter-header]
#pragma once
#include "Graphics/Anim/SpringPoint.h"

class Painter;

class RecipeCharacterView {
public:
    void Pose(Float2 target, float deltaSeconds);
    void Draw(Painter& paint) const;

private:
    SpringPoint _pose;
};
// [/recipe]
