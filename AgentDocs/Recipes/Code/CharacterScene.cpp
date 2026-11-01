// [recipe:CharacterScene]
#include "AgentDocs/Recipes/Code/CharacterScene.h"
#include <memory>

namespace {
constexpr Float2 PoseOffset{12.0f, 0.0f};
} // namespace

void RecipeCharacterScene::Update(float deltaSeconds) { _view.Pose(PoseOffset, deltaSeconds); }
void RecipeCharacterScene::Draw(Painter& paint) const { _view.Draw(paint); }
SceneDefinition RecipeCharacterDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.character";
    definition.name = "Character";
    definition.category = "MyProject";
    definition.create = [](const SceneServices&, const SceneParameters&) {
        return std::make_unique<RecipeCharacterScene>();
    };
    return definition;
}
// Register with registry.Add(RecipeCharacterDefinition()) in the RunEditorWorker catalog callback.
// [/recipe]
