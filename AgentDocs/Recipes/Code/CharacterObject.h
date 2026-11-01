// [recipe:CharacterObject-header]
#pragma once
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "GameObjects/GameObject.h"

class RecipeCharacterObject final : public GameObject {
private:
    void Update(float deltaSeconds) override;
    void Draw(Painter& paint) const override;
    RecipeCharacterView _view;
};
// [/recipe]
