// [recipe:SceneTransition-header]
#pragma once
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "Scene/Scene.h"

// Constructed from the services and its own values, so the game and the editor's catalog both make it.
class RecipeScene final : public Scene {
public:
    explicit RecipeScene(const SceneServices& services);

private:
    void Update(float deltaSeconds) override;
    void Draw(Painter& paint) const override;
    RecipeCharacterView _character; // A value member: it lives and dies with the scene
};
// [/recipe]
