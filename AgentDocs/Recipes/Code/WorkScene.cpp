// [recipe:WorkScene]
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
// [/recipe]
