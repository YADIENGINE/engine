// The short examples skills show inline, compiled here so they keep working. Each region is shown by one skill.
#include <algorithm>
#include <vector>
#include "Physics/Collision/CollisionWorld.h"
#include "Simulation/StateVisitor.h"
#include "Ui/Kernel/Ui.h"
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiLabel.h"
#include "Ui/Widgets/UiLayer.h"
#include "Ui/Widgets/UiPanel.h"
#include "Ui/Widgets/UiSlider.h"

namespace {
// [recipe:SkillCollision]
enum class Layer { Player, Enemy };

// Inside simulation rules. The world is a member; positions stay in the game's own fields.
class Rules {
public:
    Rules() { _player = _world.Add(Shape::Circle(12.0f), {_playerPosition}, MaskOf(Layer::Player)); }

    void Step(float seconds) {
        _playerPosition.x += 240.0f * seconds;
        _world.Move(_player, {_playerPosition}); // Wherever the game changes a position
        const Float2 move{0.0f, -3000.0f * seconds};
        std::erase_if(_bullets, [&](Float2& bullet) { // Fast things ask with Sweep and move only on a miss
            const auto hit = _world.Sweep(Shape::Circle(4.0f), {bullet}, move, MaskOf(Layer::Enemy));
            if (!hit) {
                bullet += move;
                return false;
            }
            _world.Remove(hit->id); // hit->owner is what Add was given; for a pool item, its handle
            return true;
        });
    }
    void State(StateVisitor& state) const { state.Field("collision", _world); }

private:
    CollisionWorld _world;
    ColliderId _player;
    Float2 _playerPosition{400.0f, 500.0f};
    std::vector<Float2> _bullets; // Bullets only ask, so they need no collider
};
// [/recipe]
} // namespace

// [recipe:SkillUi]
// A screen is a function called between _ui.Begin(seconds) and _ui.End(); it reads and writes the game's values.
void PauseScreen(Ui& ui, bool& paused, float& volume, bool& quit) {
    if (auto layer = UiLayer(ui, "pause", paused, {.alignX = UiAlign::Center, .alignY = UiAlign::Center})) {
        auto panel = UiPanel(ui, "panel", {.alignX = UiAlign::Center});
        UiLabel(ui, "Paused");
        (void)UiSlider(ui, "volume", "Volume", volume, 0.0f, 1.0f);
        if (UiButton(ui, "resume", "Resume", {.width = UiFill()}) || ui.Back()) paused = false;
        if (UiButton(ui, "quit", "Quit", {.width = UiFill()})) quit = true;
    }
}
// [/recipe]
