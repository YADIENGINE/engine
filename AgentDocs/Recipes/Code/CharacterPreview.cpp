// [recipe:CharacterPreview]
#include "AgentDocs/Recipes/Code/CharacterPreview.h"
#include <memory>

namespace {
constexpr Float2 PoseOffset{12.0f, 0.0f};
} // namespace

void RecipeCharacterPreview::Update(float deltaSeconds) { _view.Pose(PoseOffset, deltaSeconds); }
void RecipeCharacterPreview::Draw(Painter& paint) const { _view.Draw(paint); }
PreviewDefinition RecipeCharacterDefinition() {
    PreviewDefinition definition;
    definition.id = "myproject.character";
    definition.name = "Character";
    definition.category = "MyProject";
    definition.create = [](const PreviewServices&, const PreviewParameters&) {
        return std::make_unique<RecipeCharacterPreview>();
    };
    return definition;
}
// Register with registry.Add(RecipeCharacterDefinition()) in the RunPreviewHost catalog callback.
// [/recipe]
