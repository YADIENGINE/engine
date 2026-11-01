// [recipe:CharacterScene-header]
#pragma once
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "Editor/Api/SceneRegistry.h"

class RecipeCharacterScene final : public EditorScene {
private:
    void Update(float deltaSeconds) override;
    void Draw(Painter& paint) const override;
    RecipeCharacterView _view;
};

[[nodiscard]] SceneDefinition RecipeCharacterDefinition();
// [/recipe]
