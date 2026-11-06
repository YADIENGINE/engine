// [recipe:GameCamera]
#include "AgentDocs/Recipes/Code/GameCamera.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include "Camera/Debug/CameraGuides.h"
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"
#include "Simulation/StateVisitor.h"

namespace {
constexpr FloatRect Field{0.0f, 0.0f, 3200.0f, 1800.0f};
constexpr float PlayerSpeed = 360.0f;
constexpr float PlayerRadius = 22.0f;
constexpr float GridStep = 160.0f;
constexpr Color4 GridColor{60, 70, 90, 255};
constexpr std::array<Color4, 2> PlayerColors{Color4{120, 220, 255, 255}, Color4{255, 150, 120, 255}};
constexpr Color4 AimColor{255, 230, 120, 255};

// Both players framed together; each camera of a split follows one with the same settings. The limits are also where
// the stage's effects play.
CameraSettings FieldCamera() {
    return {.follow = {.deadZone = {0.1f, 0.1f}, .smoothTime = 0.25f, .lookAhead = {0.25f, 0.15f}},
            .zoom = {.fit = true, .min = 0.45f, .max = 1.0f, .padding = 120.0f},
            .limits = Field};
}
std::optional<Float2> MousePoint(const DeviceInput& input) {
    if (!input.mousePosition) return std::nullopt;
    return ToFloat2(*input.mousePosition);
}

// The scroller's rules know the canvas: what one view shows decides who wakes.
constexpr FloatRect ScrollerView{0.0f, 0.0f, 1280.0f, 720.0f};
constexpr float LevelLength = 7000.0f;
constexpr float ShipY = 360.0f;
constexpr float ShipSpeed = 420.0f;
constexpr float EnemyRadius = 30.0f;
constexpr std::array<float, RecipeScrollerSimulation::EnemyCount> EnemyX{1500.0f, 2100.0f, 2800.0f, 3400.0f,
                                                                       4200.0f, 4900.0f, 5700.0f, 6500.0f};
} // namespace

RecipeCameraView::RecipeCameraView(const SceneServices& services)
    : _players{Float2{1400.0f, 900.0f}, Float2{1800.0f, 900.0f}}
    , _stage(services, {.camera = FieldCamera()}) {
    _stage.Cameras().Main().Follow(
        {{.position = _players[0], .radius = PlayerRadius}, {.position = _players[1], .radius = PlayerRadius}});
    _stage.Cameras().Snap(); // Start framed, not sliding in from the canvas's corner
}
void RecipeCameraView::SetSplit(bool split) {
    _split = split;
    // A new view starts as a copy of the main one; the stage draws each through the same passes and hears from each.
    _stage.Cameras().Split(split ? 2 : 1, CameraSplit::SideBySide, 6.0f);
}
void RecipeCameraView::Update(float seconds, const std::array<Float2, 2>& moves, std::optional<Float2> mouse,
                              bool fire) {
    for (std::size_t i = 0; i < _players.size(); ++i) {
        const Float2 moved = _players[i] + moves[i] * PlayerSpeed * seconds;
        _players[i] = {std::clamp(moved.x, Field.x, Field.Right()), std::clamp(moved.y, Field.y, Field.Bottom())};
    }
    Cameras& cameras = _stage.Cameras();
    if (_split) {
        cameras[0].Follow({{.position = _players[0], .radius = PlayerRadius}});
        cameras[1].Follow({{.position = _players[1], .radius = PlayerRadius}});
    } else {
        cameras.Main().Follow(
            {{.position = _players[0], .radius = PlayerRadius}, {.position = _players[1], .radius = PlayerRadius}});
    }
    _stage.Update(seconds);
    // The mouse in the world, through whichever view it is over: Camera.main.ScreenToWorldPoint in Unity.
    if (mouse)
        if (const Camera2D* under = cameras.At(*mouse)) _aim = under->ToWorld(*mouse);
    if (fire) _stage.Fx().Play("explosion", {.at = _aim}); // Effect positions are world positions under a game camera
}
void RecipeCameraView::DrawWorld(Painter& paint, const StagePass&) const {
    const LineStyle grid{.width = 2.0f, .color = GridColor};
    for (float x = Field.x; x <= Field.Right(); x += GridStep)
        paint.Line({x, Field.y}, {x, Field.Bottom()}, grid);
    for (float y = Field.y; y <= Field.Bottom(); y += GridStep)
        paint.Line({Field.x, y}, {Field.Right(), y}, grid);
    for (std::size_t i = 0; i < _players.size(); ++i)
        paint.Circle(_players[i], PlayerRadius, Fill(PlayerColors[i]));
    paint.Circle(_aim, 8.0f, Outline(2.0f, AimColor));
}

RecipeScrollerSimulation::RecipeScrollerSimulation(const SimulationStart&)
    : _shipX(ScrollerView.width * 0.25f)
    , _camera(ScrollerView, {.follow = {.deadZone = {0.2f, 0.0f}, .smoothTime = 0.2f, .lookAhead = {0.35f, 0.0f}},
                             .limits = FloatRect{0.0f, 0.0f, LevelLength, ScrollerView.height}}) {
    _camera.Follow({_shipX.Current(), ShipY});
    _camera.Snap();
}
void RecipeScrollerSimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    float direction = 0.0f;
    if (input.Held(RecipeScrollButton::Left)) direction -= 1.0f;
    if (input.Held(RecipeScrollButton::Right)) direction += 1.0f;
    const float x = _shipX.Current() + direction * ShipSpeed * step.seconds;
    _shipX.Set(std::clamp(x, _scrolledTo + EnemyRadius, LevelLength - EnemyRadius));

    _camera.Follow({_shipX.Current(), ShipY});
    _camera.Update(step.seconds); // step.seconds: the same math as a view's camera, on fixed steps
    // Never back: the left limit follows the view. It is state, so it is in State and limits come from it.
    _scrolledTo = std::max(_scrolledTo, _camera.VisibleArea().x);
    _camera.SetLimits(FloatRect{_scrolledTo, 0.0f, LevelLength - _scrolledTo, ScrollerView.height});

    for (std::size_t i = 0; i < EnemyX.size(); ++i)
        _awake[i] = _awake[i] || _camera.Shows({EnemyX[i], ShipY}, EnemyRadius);
}
void RecipeScrollerSimulation::State(StateVisitor& state) const {
    state.Field("shipX", _shipX);
    state.Field("scrolledTo", _scrolledTo);
    state.Field("camera", _camera);
    state.Field("awake", _awake); // awake[0], awake[1], ...
}
SimulationDefinition RecipeScrollerDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.scroller";
    definition.settings.buttons = {"Left", "Right"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeScrollerSimulation>(start); };
    return definition;
}

RecipeCameraScene::RecipeCameraScene(const SceneServices& services, bool split, bool guides)
    : _view(services) {
    _view.SetSplit(split);
    _view.SetGuides(guides);
}
void RecipeCameraScene::HandleInput(const DeviceInput& input) {
    const auto axis = [&input](InputKey negative, InputKey positive) {
        return (input.Down(positive) ? 1.0f : 0.0f) - (input.Down(negative) ? 1.0f : 0.0f);
    };
    _moves = {Float2{axis(InputKey::A, InputKey::D), axis(InputKey::W, InputKey::S)},
              Float2{axis(InputKey::Left, InputKey::Right), axis(InputKey::Up, InputKey::Down)}};
    _mouse = MousePoint(input);
    _fire = _fire || input.mousePress.has_value();
}
void RecipeCameraScene::InputCancelled() { _moves = {}; }
std::string RecipeCameraScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    if (id == "split") _view.SetSplit(std::get<bool>(value));
    if (id == "guides") _view.SetGuides(std::get<bool>(value));
    return {};
}
void RecipeCameraScene::Update(float seconds) {
    _view.Update(seconds, _moves, _mouse, _fire);
    _fire = false;
}
void RecipeCameraScene::Draw(Painter& paint) const { _view.Draw(paint); }

RecipeScrollerScene::RecipeScrollerScene(const SceneServices& services, bool guides)
    : _hosted(services.simulation, RecipeScrollerDefinition().settings, SimulationStart{.players = 1})
    , _bindings(1)
    // The level is the world the effects play in; the stage's own cameras stay unused.
    , _stage(services, {.camera = {.limits = FloatRect{0.0f, 0.0f, LevelLength, ScrollerView.height}}})
    , _guides(guides) {
    _bindings.Bind(0, RecipeScrollButton::Left, InputKey::A);
    _bindings.Bind(0, RecipeScrollButton::Right, InputKey::D);
    ShowRules(); // From the first frame
}
// The rules' camera between its last two steps, like StepSmoothed: the stage draws and hears through it.
void RecipeScrollerScene::ShowRules() {
    const std::array views{Rules().Camera().View(_hosted.Runner().Alpha())};
    _stage.SetViews(views);
}
void RecipeScrollerScene::HandleInput(const DeviceInput& input) { _hosted.Runner().Feed(_bindings.Read(input)); }
void RecipeScrollerScene::InputCancelled() { _hosted.Runner().CancelInput(); }
std::string RecipeScrollerScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    if (id == "guides") _guides = std::get<bool>(value);
    return {};
}
void RecipeScrollerScene::Update(float seconds) {
    (void)_hosted.Runner().Advance(seconds);
    ShowRules();
    _stage.Update(seconds);
}
void RecipeScrollerScene::Draw(Painter& paint) const { _stage.Draw(paint, *this); }
void RecipeScrollerScene::DrawWorld(Painter& paint, const StagePass&) const {
    const float alpha = _hosted.Runner().Alpha();
    const LineStyle grid{.width = 2.0f, .color = GridColor};
    for (float x = 0.0f; x <= LevelLength; x += GridStep)
        paint.Line({x, 0.0f}, {x, ScrollerView.height}, grid);
    for (std::size_t i = 0; i < EnemyX.size(); ++i) {
        const Color4 color = Rules().Awake(i) ? Color4{255, 110, 110, 255} : Color4{110, 110, 130, 255};
        paint.Circle({EnemyX[i], ShipY}, EnemyRadius, Fill(color));
    }
    paint.Circle({Rules().ShipX(alpha), ShipY}, PlayerRadius, Fill(PlayerColors[0]));
}
void RecipeScrollerScene::DrawHud(Painter& paint) const {
    if (_guides) DrawCameraGuides(paint, Rules().Camera()); // The stage shows guides only for its own cameras
}

SceneDefinition RecipeCameraSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.camera";
    definition.name = "Camera";
    definition.category = "MyProject";
    definition.description = "WASD and arrows move two players; a click fires at the mouse.";
    definition.parameters = {{"split", "Split screen", SceneParameterKind::Boolean, false},
                             {"guides", "Camera guides", SceneParameterKind::Boolean, true}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipeCameraScene>(services, std::get<bool>(parameters.at("split")),
                                                   std::get<bool>(parameters.at("guides")));
    };
    return definition;
}
SceneDefinition RecipeScrollerSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.scroller";
    definition.name = "Scroller";
    definition.category = "MyProject";
    definition.description = "A and D move; the view never scrolls back, and enemies wake as it reaches them.";
    definition.parameters = {{"guides", "Camera guides", SceneParameterKind::Boolean, true}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipeScrollerScene>(services, std::get<bool>(parameters.at("guides")));
    };
    return definition;
}
// [/recipe]
