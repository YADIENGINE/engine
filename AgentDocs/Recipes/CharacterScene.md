Open the game's own scene in the editor's catalog: one class, no editor copy.

APIs: Core/Scene/Scene.h, Core/Scene/SceneServices.h, Editor/Api/SceneRegistry.h; scene: AgentDocs/Recipes/Code/SceneTransition.h.

Register the definition in the catalog callback passed to Editor/Worker/WorkerHost.h RunEditorWorker; build the worker with engine_add_editor_worker.

<!-- recipe:CharacterScene-header -->
```cpp
#pragma once
#include "Editor/Api/SceneRegistry.h"

// The game's own scene (RecipeScene) in the editor's catalog: no second copy for the editor.
[[nodiscard]] SceneDefinition RecipeCharacterDefinition();
```
<!-- /recipe -->

<!-- recipe:CharacterScene -->
```cpp
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
```
<!-- /recipe -->

Pitfalls: The worker links the scene's library and excludes the game factory. The scene's constructor takes `const SceneServices&` plus its own values, so the game (`Game::Services()`) and the factory build it alike. Never write a second scene for the editor; paint.Time() follows editor Pause.

Concept: `Docs/Editor/GettingStarted.md#cmake-target`.
