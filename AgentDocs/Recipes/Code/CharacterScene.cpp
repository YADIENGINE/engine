// [recipe:CharacterScene]
#include "AgentDocs/Recipes/Code/CharacterScene.h"
#include <memory>
#include "AgentDocs/Recipes/Code/SceneTransition.h"

SceneDefinition RecipeCharacterDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.character";
    definition.name = "Character";
    definition.category = "MyProject";
    definition.create = [](const SceneServices& services, const SceneParameters&) {
        return std::make_unique<RecipeScene>(services);
    };
    return definition;
}
// Register with registry.Add(RecipeCharacterDefinition()) in the RunEditorWorker catalog callback.
// [/recipe]
