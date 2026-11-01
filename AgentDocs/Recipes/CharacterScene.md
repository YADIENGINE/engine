Scene the production character View through the public editor API.

APIs: Editor/Api/EditorScene.h, Editor/Api/SceneRegistry.h; production View: AgentDocs/Recipes/Code/AnimatedCharacter.h.

Register the definition in the catalog callback passed to Editor/Worker/WorkerHost.h RunEditorWorker; build the worker with engine_add_editor_worker.

<!-- recipe:CharacterScene-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:CharacterScene -->
```cpp
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
```
<!-- /recipe -->

Pitfalls: The worker links the production View library and excludes the game factory. Do not redraw a separate editor scene copy; paint.Time() follows editor Pause.

Concept: `Docs/Editor/GettingStarted.md#cmake-target`.
