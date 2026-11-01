Preview the production character View through the public editor API.

APIs: Editor/Api/EditorPreview.h, Editor/Api/PreviewRegistry.h; production View: AgentDocs/Recipes/Code/AnimatedCharacter.h.

Register the definition in the catalog callback passed to Editor/Preview/PreviewHost.h RunPreviewHost; build the worker with engine_add_editor_worker.

<!-- recipe:CharacterPreview-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:CharacterPreview -->
```cpp
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
```
<!-- /recipe -->

Pitfalls: The worker links the production View library and excludes the game factory. Do not redraw a separate preview copy; paint.Time() follows editor Pause.

Concept: [guide](../../Docs/Editor/GettingStarted.md#cmake-target).
