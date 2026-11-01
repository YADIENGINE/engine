Register an owned GameObject in a scene with safe shutdown.

APIs: Core/Scene/Scene.h, Core/GameObjects/GameObject.h; production View: AgentDocs/Recipes/Code/AnimatedCharacter.h.

Use this scene with `Recipes/SceneTransition.md`.

<!-- recipe:SceneTransition-header -->
```cpp
#pragma once
#include "Scene/Scene.h"

class RecipeScene final : public Scene {
public:
    RecipeScene();
    ~RecipeScene() override;
};
```
<!-- /recipe -->

<!-- recipe:SceneTransition -->
```cpp
#include "AgentDocs/Recipes/Code/SceneTransition.h"
#include <memory>
#include "AgentDocs/Recipes/Code/CharacterObject.h"

RecipeScene::RecipeScene()
    : Scene(TimeDomain::Game) {
    AddObject(std::make_unique<RecipeCharacterObject>());
}
RecipeScene::~RecipeScene() { ShutdownNoThrow(); }
```
<!-- /recipe -->

<!-- recipe:CharacterObject-header -->
```cpp
#pragma once
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "GameObjects/GameObject.h"

class RecipeCharacterObject final : public GameObject {
private:
    void Update(float deltaSeconds) override;
    void Draw(Painter& paint) const override;
    RecipeCharacterView _view;
};
```
<!-- /recipe -->

<!-- recipe:CharacterObject -->
```cpp
#include "AgentDocs/Recipes/Code/CharacterObject.h"

namespace {
constexpr Float2 PoseOffset{12.0f, 0.0f};
} // namespace

void RecipeCharacterObject::Update(float deltaSeconds) { _view.Pose(PoseOffset, deltaSeconds); }
void RecipeCharacterObject::Draw(Painter& paint) const { _view.Draw(paint); }
```
<!-- /recipe -->

Pitfalls: Call ShutdownNoThrow in the concrete scene destructor while members and hooks are alive. Start is once per object; Enter is once per scene visit.

Concept: `Docs/Runtime.md#lifecycle`.
