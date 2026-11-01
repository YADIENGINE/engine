Create or reuse one focused scene for the current request: `Editor/Work/<Task>Scene.cpp`.
Follow the [visual-change skill](../Skills/visual-change/SKILL.md).

Use the production View/model and show only the subject. Expose values the developer will likely tweak as parameters.
Keep randomness off unless requested; when needed, expose an explicit seed and apply it on reset.
Return focus bounds around the subject from `Inspect()` so Focus content frames it.
Use a stable ID `<project>.work.<task>`, `WorkSceneCategory` and `WorkSceneCategoryOrder`.

The project's worker opts in once:

```cmake
engine_add_editor_worker(MyProjectEditorWorker
    SOURCES Editor/Main.cpp
    SCENE_DIRS Editor/Work
    LIBRARIES MyProjectViews)
```

`SCENE_DIRS` globs each directory's immediate `*.cpp` files with `CONFIGURE_DEPENDS` and links them as OBJECT libraries.
New files need no catalog or CMake edits: Build & Refresh / `python Engine refresh` picks them up.
Keep these files out of `SOURCES` and other source globs to avoid compiling them twice.
`RunEditorWorker` adds registered makers before the project catalog callback; duplicate IDs fail with the ID in the error.
Registration stores the maker only: do not load assets, draw or create a scene during static initialization.
The worker must link the production View/model library (not the game's entry point).

This compiled example uses the same `RecipeCharacterView` as the character-object recipe. Replace its include/type with
the project's production View. The optional random-pose controls demonstrate seeded variation; remove them when unused.

<!-- recipe:WorkScene -->
```cpp
#include <memory>
#include <random>
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "Editor/Api/SceneAutoRegistration.h"

namespace {
constexpr float RandomPoseRange = 16.0f;
// Includes the View's ears, sway and full pose range, with margin.
constexpr SceneContentBounds SubjectBounds{40, 50, 240, 180};

class CharacterWorkScene final : public EditorScene {
public:
    explicit CharacterWorkScene(const SceneParameters& parameters)
        : _offset(static_cast<float>(std::get<double>(parameters.at("offset")))) {
        // Randomness is opt-in and replayable. Omit these controls for deterministic subjects.
        if (std::get<bool>(parameters.at("randomPose"))) {
            std::mt19937 random(static_cast<std::mt19937::result_type>(std::get<std::int64_t>(parameters.at("seed"))));
            _randomOffset = std::uniform_real_distribution<float>(-RandomPoseRange, RandomPoseRange)(random);
        }
    }
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override {
        if (id != "offset") return "Unknown parameter";
        _offset = static_cast<float>(std::get<double>(value));
        return {};
    }
    [[nodiscard]] SceneInspection Inspect() const override { return {.focus = SubjectBounds}; }

private:
    void Update(float deltaSeconds) override { _view.Pose({_offset + _randomOffset, 0.0f}, deltaSeconds); }
    void Draw(Painter& paint) const override { _view.Draw(paint); }
    RecipeCharacterView _view; // Production View, also used by the game's character object.
    float _offset;
    float _randomOffset = 0.0f;
};

SceneDefinition MakeCharacterWorkScene() {
    SceneDefinition definition;
    definition.id = "myproject.work.character-pose";
    definition.name = "Character pose";
    definition.category = WorkSceneCategory;
    definition.description = "Isolated production character pose for this task.";
    definition.categoryOrder = WorkSceneCategoryOrder;
    definition.parameters = {
        {"offset", "Pose offset", SceneParameterKind::Float, 12.0, -48.0, 48.0},
        {"randomPose", "Random pose", SceneParameterKind::Boolean, false, 0.0, 1.0, SceneApplyMode::OnReset},
        {"seed", "Seed", SceneParameterKind::Integer, std::int64_t{1729}, 0.0, 1000000.0, SceneApplyMode::OnReset}};
    definition.create = [](const SceneServices&, const SceneParameters& parameters) {
        return std::make_unique<CharacterWorkScene>(parameters);
    };
    return definition;
}

[[maybe_unused]] const bool Registered = RegisterEditorScene(&MakeCharacterWorkScene);
} // namespace
```
<!-- /recipe -->

After refresh, `python Engine ctl open myproject.work.character-pose`, then use `ctl set`, `ctl seek`,
`ctl screenshot --scale 0.5` or `ctl frames`. Read the resulting PNGs; leave the scene open for the developer.
When the developer declares the task done, ask whether to delete this scene or move it to a permanent category.

APIs: [SceneAutoRegistration.h](../../Editor/Api/SceneAutoRegistration.h),
[SceneRegistry.h](../../Editor/Api/SceneRegistry.h), [EditorScene.h](../../Editor/Api/EditorScene.h).
Concept: [Connecting a worker](../../Docs/Editor/GettingStarted.md#cmake-target).
