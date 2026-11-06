Keep a view as a value member of a scene and call it from the scene's Update and Draw.

APIs: Core/Scene/Scene.h; the view: AgentDocs/Recipes/Code/AnimatedCharacter.h.

Use this scene with `Recipes/SceneTransition.md`; `Recipes/CharacterScene.md` opens it in the editor.

<!-- recipe:SceneTransition-header -->
```cpp
#pragma once
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "Scene/Scene.h"

// Constructed from the services and its own values, so the game and the editor's catalog both make it.
class RecipeScene final : public Scene {
public:
    explicit RecipeScene(const SceneServices& services);

private:
    void Update(float deltaSeconds) override;
    void Draw(Painter& paint) const override;
    RecipeCharacterView _character; // A value member: it lives and dies with the scene
};
```
<!-- /recipe -->

<!-- recipe:SceneTransition -->
```cpp
#include "AgentDocs/Recipes/Code/SceneTransition.h"

namespace {
constexpr Float2 PoseOffset{12.0f, 0.0f};
} // namespace

RecipeScene::RecipeScene(const SceneServices&)
    : Scene(TimeDomain::Game) {}
void RecipeScene::Update(float deltaSeconds) { _character.Pose(PoseOffset, deltaSeconds); }
void RecipeScene::Draw(Painter& paint) const { _character.Draw(paint); }
```
<!-- /recipe -->

Pitfalls: A scene has no object list: its parts are value members, and the scene calls them in the order it wants. Things that spawn and die belong in a `Pool<T>` (`Recipes/Pool.md`). The scene needs no destructor; its host shuts it down while it is alive. Start runs once per scene; Enter runs on every visit.

Concept: `Docs/Runtime.md#lifecycle`.
