# Depth stage recipe

A 2.5D street: two players walk among trees in depth order, jump with shadows that stay on the ground, leave dust where they land, and hills drift behind at their own depth. Split screen gives each player a view through the same passes.

APIs: Core/Stage/Stage.h, Core/Stage/StageView.h, Core/Stage/DepthOrder.h, Core/Graphics/Draw/Elevation.h, Core/Fx/FxStage.h (`Mark`), Core/Camera/CameraView.h (`Parallaxed`). Concepts: [Stage](../../Docs/Graphics/Stage.md).

The scene says what is there, pass by pass; the stage keeps the order and repeats it for each view. A camera that lives in a simulation is in the [game camera recipe](GameCamera.md) (part 2).

<!-- recipe:DepthStage-header -->
```cpp
#pragma once
#include <array>
#include <cstddef>
#include <string>
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"
#include "Stage/DepthOrder.h"
#include "Stage/Stage.h"

// A 2.5D street seen from the side and a little above: two players walk among trees, jump with shadows under them and
// leave dust where they land; hills drift behind at their own depth. Split screen gives each player a view, drawn
// through the same passes. View code on seconds; in a game the walkers would be a simulation's, read here.
struct RecipeWalker {
    Float2 ground{};     // Where it stands: what it sorts by and where its shadow lies
    float height = 0.0f; // How far above the ground it is
    float rise = 0.0f;   // Upward speed, world units per second
};

// What the world pass sorts: a key of the game's own, so several kinds share one order.
struct RecipeStreetThing {
    enum class Kind { Walker, Tree };
    Kind kind = Kind::Walker;
    std::size_t index = 0;
};

class RecipeStreetScene final : public Scene, StageView {
public:
    RecipeStreetScene(const SceneServices& services, bool split);
    ~RecipeStreetScene() override { ShutdownNoThrow(); }
    [[nodiscard]] const RecipeWalker& Walker(std::size_t player) const { return _walkers[player]; }
    [[nodiscard]] const Cameras& Views() const { return _stage.Cameras(); }

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void UpdateReal(float realSeconds) override { _stage.UpdateReal(realSeconds); } // Even in a pause
    void Update(float seconds) override;
    void Draw(Painter& paint) const override { _stage.Draw(paint, *this); }
    // The passes this scene fills; the stage calls them in its order, for each view.
    void DrawBackground(Painter& paint, const StagePass& pass) const override;
    void DrawFloor(Painter& paint, const StagePass& pass) const override;
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    void DrawHud(Painter& paint) const override;
    void SetSplit(bool split);

    Stage _stage;
    std::array<RecipeWalker, 2> _walkers;
    std::array<Float2, 2> _moves{};
    std::array<bool, 2> _jumps{};
    bool _split = false;
    mutable DepthOrder<RecipeStreetThing> _order; // Refilled in each world pass; a member keeps its memory
};

[[nodiscard]] SceneDefinition RecipeStreetSceneDefinition();
```
<!-- /recipe -->

<!-- recipe:DepthStage -->
```cpp
#include "AgentDocs/Recipes/Code/DepthStage.h"
#include <algorithm>
#include <memory>
#include "Graphics/Draw/Elevation.h"
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"

namespace {
// The street: y is depth, from the far kerb (0) to the near one. The camera may look above it, at the sky and hills.
constexpr FloatRect Street{0.0f, 0.0f, 4000.0f, 400.0f};
constexpr FloatRect Seen{0.0f, -360.0f, 4000.0f, 760.0f};
constexpr float WalkSpeed = 300.0f;
constexpr float JumpSpeed = 520.0f;
constexpr float Gravity = 1600.0f;
constexpr float BodyRadius = 22.0f;
constexpr std::array<Float2, 6> Trees{Float2{500.0f, 60.0f}, {900.0f, 300.0f}, {1400.0f, 120.0f},
                                      {2100.0f, 340.0f},     {2600.0f, 90.0f}, {3300.0f, 220.0f}};
constexpr std::array<Color4, 2> WalkerColors{Color4{120, 220, 255, 255}, Color4{255, 150, 120, 255}};
constexpr Color4 Sky = Rgb(0x9cc3de);
constexpr Color4 FarHills = Rgb(0x7f9db0);
constexpr Color4 NearHills = Rgb(0x5f8170);
constexpr Color4 Pavement = Rgb(0x8d8a80);
constexpr Color4 Kerb = Rgb(0x6d6a62);
constexpr Color4 Trunk = Rgb(0x6b4a33);
constexpr Color4 Leaves = Rgb(0x3f7a45);
constexpr Color4 Dust{120, 110, 95, 110};

// Far hills move a fifth as fast as the street, near ones half as fast. Their feet reach below the horizon, where the
// street drawn next covers them, so no gap shows at any zoom.
void DrawHills(Painter& paint, const StagePass& pass, float depth, Color4 color, float spacing, float size) {
    auto layer = paint.Camera(pass.view.Parallaxed(depth));
    for (float x = -2.0f * spacing; x < Seen.Right() * depth + 3.0f * spacing; x += spacing)
        paint.Ellipse({x, 40.0f}, {spacing * 0.8f, size}, Fill(color));
}
} // namespace

RecipeStreetScene::RecipeStreetScene(const SceneServices& services, bool split)
    : _stage(services, {.camera = {.follow = {.smoothTime = 0.25f, .lookAhead = {0.3f, 0.0f}},
                                   .zoom = {.fit = true, .min = 0.6f, .max = 1.0f, .padding = 160.0f},
                                   .limits = Seen}})
    , _walkers{RecipeWalker{.ground = {300.0f, 200.0f}}, RecipeWalker{.ground = {500.0f, 260.0f}}} {
    SetSplit(split);
    _stage.Cameras().Snap();
}

void RecipeStreetScene::SetSplit(bool split) {
    _split = split;
    _stage.Cameras().Split(split ? 2 : 1, CameraSplit::Stacked, 6.0f);
    if (split) {
        _stage.Cameras()[0].Follow(_walkers[0].ground);
        _stage.Cameras()[1].Follow(_walkers[1].ground);
    } else {
        _stage.Cameras().Main().Follow({{.position = _walkers[0].ground}, {.position = _walkers[1].ground}});
    }
}

void RecipeStreetScene::HandleInput(const DeviceInput& input) {
    const auto axis = [&input](InputKey negative, InputKey positive) {
        return (input.Down(positive) ? 1.0f : 0.0f) - (input.Down(negative) ? 1.0f : 0.0f);
    };
    _moves = {Float2{axis(InputKey::A, InputKey::D), axis(InputKey::W, InputKey::S)},
              Float2{axis(InputKey::Left, InputKey::Right), axis(InputKey::Up, InputKey::Down)}};
    _jumps[0] = _jumps[0] || input.Pressed(InputKey::Space);
    _jumps[1] = _jumps[1] || input.Pressed(InputKey::Enter);
}
void RecipeStreetScene::InputCancelled() { _moves = {}; }

std::string RecipeStreetScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    if (id == "split") SetSplit(std::get<bool>(value));
    return {};
}

void RecipeStreetScene::Update(float seconds) {
    for (std::size_t i = 0; i < _walkers.size(); ++i) {
        RecipeWalker& walker = _walkers[i];
        const Float2 moved = walker.ground + _moves[i] * (WalkSpeed * seconds);
        walker.ground = {std::clamp(moved.x, Street.x, Street.Right()), std::clamp(moved.y, Street.y, Street.Bottom())};
        if (_jumps[i] && walker.height == 0.0f) walker.rise = JumpSpeed;
        _jumps[i] = false;
        if (walker.height > 0.0f || walker.rise > 0.0f) {
            walker.rise -= Gravity * seconds;
            walker.height += walker.rise * seconds;
            if (walker.height <= 0.0f) { // Landed: dust lies on the ground, under everyone
                walker.height = 0.0f;
                walker.rise = 0.0f;
                _stage.Fx().Mark(walker.ground, Dust, {.radius = 30.0f, .seconds = 2.0f, .flatten = 0.35f});
            }
        }
    }
    // Cameras follow the ground, not the jump, so the view does not bob.
    if (_split) {
        _stage.Cameras()[0].Follow(_walkers[0].ground);
        _stage.Cameras()[1].Follow(_walkers[1].ground);
    } else {
        _stage.Cameras().Main().Follow({{.position = _walkers[0].ground}, {.position = _walkers[1].ground}});
    }
    _stage.Update(seconds);
}

void RecipeStreetScene::DrawBackground(Painter& paint, const StagePass& pass) const {
    {
        auto sky = paint.Clip(pass.view.viewport); // Fixed to the screen
        paint.Rect(pass.view.viewport, Fill(Sky));
    }
    DrawHills(paint, pass, 0.2f, FarHills, 420.0f, 150.0f);
    DrawHills(paint, pass, 0.5f, NearHills, 300.0f, 100.0f);
}

void RecipeStreetScene::DrawFloor(Painter& paint, const StagePass&) const {
    paint.Rect({Seen.x, Street.y, Seen.width, 1000.0f}, Fill(Pavement)); // On past the near kerb when zoomed out
    paint.Rect({Street.x, Street.y - 12.0f, Street.width, 12.0f}, Fill(Kerb));
    // Every shadow lies on the ground before anything stands, so none falls over a character.
    for (const Float2 tree : Trees)
        DrawGroundShadow(paint, tree, 0.0f, {.radius = 46.0f});
    for (const RecipeWalker& walker : _walkers)
        DrawGroundShadow(paint, walker.ground, walker.height, {.radius = BodyRadius});
}

void RecipeStreetScene::DrawWorld(Painter& paint, const StagePass&) const {
    // Nearer (lower) things go later; level ones keep their order, so they never flicker in front of each other.
    _order.Clear();
    for (std::size_t i = 0; i < Trees.size(); ++i)
        _order.Add(Trees[i].y, {RecipeStreetThing::Kind::Tree, i});
    for (std::size_t i = 0; i < _walkers.size(); ++i)
        _order.Add(_walkers[i].ground.y, {RecipeStreetThing::Kind::Walker, i});
    for (const RecipeStreetThing& thing : _order.Sorted()) {
        if (thing.kind == RecipeStreetThing::Kind::Tree) {
            const Float2 foot = Trees[thing.index];
            paint.Rect({foot.x - 8.0f, foot.y - 70.0f, 16.0f, 70.0f}, Fill(Trunk));
            paint.Circle(foot - Float2{0.0f, 100.0f}, 46.0f, Fill(Leaves));
            continue;
        }
        const RecipeWalker& walker = _walkers[thing.index];
        auto lifted = Raised(paint, walker.height); // Drawn at its ground point, shown above it
        paint.Circle(walker.ground - Float2{0.0f, BodyRadius}, BodyRadius, Fill(WalkerColors[thing.index]));
    }
}

void RecipeStreetScene::DrawHud(Painter& paint) const {
    // Canvas coordinates: a mark of each player's colour in the corner of the view that follows them.
    for (std::size_t i = 0; i < _walkers.size(); ++i) {
        const FloatRect& view = _stage.Cameras()[_split ? i : 0].Viewport();
        paint.Circle({view.x + 24.0f + (_split ? 0.0f : 28.0f * static_cast<float>(i)), view.y + 24.0f}, 10.0f,
                     Fill(WalkerColors[i]));
    }
}

SceneDefinition RecipeStreetSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.street";
    definition.name = "Street";
    definition.category = "MyProject";
    definition.description = "WASD and Space, arrows and Enter: walk and jump among the trees.";
    definition.parameters = {{"split", "Split screen", SceneParameterKind::Boolean, false}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipeStreetScene>(services, std::get<bool>(parameters.at("split")));
    };
    return definition;
}
```
<!-- /recipe -->

Look at it: `python Engine ctl open myproject.street --set split=true --screenshot`.

Pitfalls: Sort by the point a thing stands on, never by where it is drawn: a jump lifts it up the screen but does not bring it nearer. Add things to `DepthOrder` in the same order every frame, so level ones keep their places. Draw shadows in `DrawFloor`, not beside each thing in `DrawWorld`, or a shadow falls over whoever stands just behind. The background is drawn outside `paint.Camera`; each layer opens its own with `pass.view.Parallaxed(depth)`, and its lower edge should reach below the horizon, where the floor covers it. Cameras follow the ground point, or the view bobs with every jump. Never write the passes yourself around a `Stage`: a later engine pass then lands in the right place without a change to the scene.
