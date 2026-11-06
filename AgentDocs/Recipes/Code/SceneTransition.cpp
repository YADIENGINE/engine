// [recipe:SceneTransition]
#include "AgentDocs/Recipes/Code/SceneTransition.h"
#include <memory>
#include "AgentDocs/Recipes/Code/CharacterObject.h"

RecipeScene::RecipeScene(const SceneServices&)
    : Scene(TimeDomain::Game) {
    AddObject(std::make_unique<RecipeCharacterObject>());
}
RecipeScene::~RecipeScene() { ShutdownNoThrow(); }
// [/recipe]
