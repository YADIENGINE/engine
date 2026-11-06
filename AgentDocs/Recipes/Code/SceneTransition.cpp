// [recipe:SceneTransition]
#include "AgentDocs/Recipes/Code/SceneTransition.h"

namespace {
constexpr Float2 PoseOffset{12.0f, 0.0f};
} // namespace

RecipeScene::RecipeScene(const SceneServices&)
    : Scene(TimeDomain::Game) {}
void RecipeScene::Update(float deltaSeconds) { _character.Pose(PoseOffset, deltaSeconds); }
void RecipeScene::Draw(Painter& paint) const { _character.Draw(paint); }
// [/recipe]
