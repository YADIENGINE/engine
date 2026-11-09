// [recipe:WalkerSlide-header]
#pragma once
#include "Physics/Collision/CollisionWorld.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Geometry/Float2.h"

enum class RecipeWalkerButton { Left, Right, Up, Down };
enum class RecipeWalkerLayer { Feet, Wall };

// The rules, in Game/Simulation: top-down walking in a room. The feet stop at walls, slide along them, also along a
// slanted one, and find a doorway without exact aim.
class RecipeWalkerSimulation final : public Simulation {
public:
    explicit RecipeWalkerSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] Float2 Feet() const { return _feet; }
    [[nodiscard]] bool AgainstWall() const { return _againstWall; }
    [[nodiscard]] const CollisionWorld& Collision() const { return _collision; }

private:
    CollisionWorld _collision;
    ColliderId _feetCollider;
    Float2 _feet{153.0f, 60.0f};
    bool _againstWall = false;
};

[[nodiscard]] SimulationDefinition RecipeWalkerDefinition();
// [/recipe]
