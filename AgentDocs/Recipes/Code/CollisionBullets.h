// [recipe:CollisionBullets-header]
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Input/InputBindings.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"

class Painter;

enum class RecipeLayer { Player, Enemy, Wall };
enum class RecipeShooterButton { Left, Right, Fire };

// The rules, in Game/Simulation: colliders are found through the world, positions stay in the game's own fields.
class RecipeShooterSimulation final : public Simulation {
public:
    explicit RecipeShooterSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] int Score() const { return _score; }
    [[nodiscard]] int Touches() const { return _touches; }
    [[nodiscard]] float PlayerX() const { return _playerX; }
    [[nodiscard]] const CollisionWorld& Collision() const { return _collision; }
    [[nodiscard]] const std::vector<Float2>& Bullets() const { return _bullets; }

private:
    CollisionWorld _collision;
    ColliderId _player;
    std::vector<Float2> _bullets; // Bullets only ask questions, so they need no collider of their own
    float _playerX = 400.0f;
    int _score = 0;
    int _touches = 0;
};

[[nodiscard]] SimulationDefinition RecipeShooterDefinition();
// The view, outside Game/Simulation. A debug switch shows what the rules see on top of what the player sees.
void DrawRecipeShooter(Painter& paint, const RecipeShooterSimulation& simulation, bool showColliders);

// An editor scene with the switch as a parameter: `ctl open ID --set showColliders=true --screenshot`.
class RecipeShooterEditorScene final : public EditorScene {
public:
    RecipeShooterEditorScene(const SceneServices& services, std::uint64_t seed, bool showColliders);

private:
    void HandleInput(const DeviceInput& input) override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    HostedSimulation<RecipeShooterSimulation> _hosted;
    InputBindings _bindings;
    bool _showColliders;
};
[[nodiscard]] SceneDefinition RecipeShooterSceneDefinition();
// [/recipe]
