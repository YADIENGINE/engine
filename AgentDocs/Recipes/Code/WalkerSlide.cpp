// [recipe:WalkerSlide]
#include "AgentDocs/Recipes/Code/WalkerSlide.h"
#include <array>
#include <memory>
#include "Physics/Collision/MoveAndSlide.h"
#include "Simulation/StateVisitor.h"

namespace {
constexpr float WalkSpeed = 60.0f; // Units per second
const Shape FeetShape = Shape::Circle(4.0f);
// Walls stop the feet. Walking straight at a wall with a doorway up to 6 units to one side moves the feet into it.
// Were the feet's own collider on a layer in the mask, `.ignore = _feetCollider` would keep it from stopping itself.
constexpr SlideOptions Walking{.mask = MaskOf(RecipeWalkerLayer::Wall), .cornerAssist = 6.0f};
} // namespace

RecipeWalkerSimulation::RecipeWalkerSimulation(const SimulationStart& /*start*/) {
    const auto wall = MaskOf(RecipeWalkerLayer::Wall);
    // A wall across the room, y = 100..110, with a doorway 12 units wide between x = 154 and x = 166.
    (void)_collision.Add(Shape::Box(77.0f, 5.0f), {{77.0f, 105.0f}}, wall);
    (void)_collision.Add(Shape::Box(77.0f, 5.0f), {{243.0f, 105.0f}}, wall);
    // A slanted wall in a corner: the feet slide along it instead of sticking.
    const std::array<Float2, 3> slope{Float2{250.0f, 0.0f}, Float2{320.0f, 0.0f}, Float2{320.0f, 70.0f}};
    (void)_collision.Add(Shape::Polygon(slope), {}, wall);
    _feetCollider = _collision.Add(FeetShape, {_feet}, MaskOf(RecipeWalkerLayer::Feet));
}
void RecipeWalkerSimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    const auto axis = [&](RecipeWalkerButton less, RecipeWalkerButton more) {
        return (input.Held(more) ? 1.0f : 0.0f) - (input.Held(less) ? 1.0f : 0.0f);
    };
    const Float2 direction{axis(RecipeWalkerButton::Left, RecipeWalkerButton::Right),
                           axis(RecipeWalkerButton::Up, RecipeWalkerButton::Down)};
    _againstWall = false;
    if (direction == Float2{}) return;
    const auto slide =
        MoveAndSlide(_collision, FeetShape, {_feet}, Normalized(direction) * (WalkSpeed * step.seconds), Walking);
    _feet = slide.position;
    _againstWall = slide.blocked; // slide.Hits() tells what stopped it, and which way that faces
    _collision.Move(_feetCollider, {_feet}); // The game keeps the position; the world has to be told
}
void RecipeWalkerSimulation::State(StateVisitor& state) const {
    state.Field("feet", _feet);
    state.Field("againstWall", _againstWall);
    state.Field("collision", _collision);
}
SimulationDefinition RecipeWalkerDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.walker";
    definition.settings.buttons = {"Left", "Right", "Up", "Down"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeWalkerSimulation>(start); };
    return definition;
}
// [/recipe]
