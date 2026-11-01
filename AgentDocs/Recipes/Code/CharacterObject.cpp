// [recipe:CharacterObject]
#include "AgentDocs/Recipes/Code/CharacterObject.h"

namespace {
constexpr Float2 PoseOffset{12.0f, 0.0f};
} // namespace

void RecipeCharacterObject::Update(float deltaSeconds) { _view.Pose(PoseOffset, deltaSeconds); }
void RecipeCharacterObject::Draw(Painter& paint) const { _view.Draw(paint); }
// [/recipe]
