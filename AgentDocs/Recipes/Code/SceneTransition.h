// [recipe:SceneTransition-header]
#pragma once
#include "Scene/Scene.h"

// Constructed from the services and its own values, so the game and the editor's catalog both make it.
class RecipeScene final : public Scene {
public:
    explicit RecipeScene(const SceneServices& services);
    ~RecipeScene() override;
};
// [/recipe]
