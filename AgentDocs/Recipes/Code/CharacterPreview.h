// [recipe:CharacterPreview-header]
#pragma once
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "Editor/Api/PreviewRegistry.h"

class RecipeCharacterPreview final : public EditorPreview {
private:
    void Update(float deltaSeconds) override;
    void Draw(Painter& paint) const override;
    RecipeCharacterView _view;
};

[[nodiscard]] PreviewDefinition RecipeCharacterDefinition();
// [/recipe]
