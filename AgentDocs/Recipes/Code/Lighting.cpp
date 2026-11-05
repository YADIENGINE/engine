// [recipe:Lighting]
#include "AgentDocs/Recipes/Code/Lighting.h"
#include <array>
#include <cmath>
#include <memory>
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"

namespace {
constexpr Color4 Floor = Rgb(0x8a8378);
constexpr Color4 Crate = Rgb(0xb5793a);
constexpr Color4 Neon = Rgb(0x30e0ff);
constexpr Color4 Night = Rgb(0x10121c);
constexpr Color4 LanternColor = Rgb(0xffd9a0);
constexpr Color4 TorchColor = Rgb(0xff8a3c);
constexpr FloatRect Room{0.0f, 0.0f, 1600.0f, 1000.0f};
constexpr Float2 Torch{1100.0f, 300.0f};
constexpr Float2 Sign{500.0f, 280.0f};
constexpr float PlayerSpeed = 320.0f;
} // namespace

RecipeLitView::RecipeLitView(Float2 canvas)
    : _canvas(canvas)
    , _player(Room.Center())
    , _aim(Room.Center() + Float2{100.0f, 0.0f}) {}

void RecipeLitView::Update(float seconds, Float2 move, Float2 mouse) {
    _time += seconds;
    _player += move * (PlayerSpeed * seconds);
    _aim = View().ToWorld(mouse);
}

CameraView RecipeLitView::View() const { return {.offset = _canvas * 0.5f, .target = _player}; }

void RecipeLitView::DrawWorld(Painter& paint) const {
    paint.Rect(Room, Fill(Floor));
    paint.Rect(FloatRect::FromCenter({1000.0f, 600.0f}, {120.0f, 120.0f}), Fill(Crate));
    paint.Circle(_player, 18.0f, Fill(Rgb(0xe8ecff)));
}

void RecipeLitView::Draw(Painter& paint) const {
    const CameraView view = View();
    {
        auto world = paint.Camera(view);
        DrawWorld(paint);
    }

    // A light's strength can be any function of time: this one flickers. A tween or a clip can drive it too.
    const float flicker = 0.85f + 0.15f * std::sin(_time * 13.0f) * std::sin(_time * 7.3f);
    const Float2 aimed = _aim - _player;
    const std::array lights{
        LightSource{.at = _player, .radius = 160.0f, .color = LanternColor, .intensity = 0.7f},
        LightSource{.at = _player,
                    .radius = 520.0f,
                    .cone = {.direction = std::atan2(aimed.y, aimed.x), .halfAngle = 0.35f}},
        LightSource{.at = Torch, .radius = 300.0f * flicker, .color = TorchColor, .core = 0.15f},
    };
    const LightFrame frame{.ambient = Night, .lights = lights};
    _lights.Draw(paint, frame, view); // Outside paint.Camera, with the view the world was drawn through

    {
        // What shines by itself goes after the layer, so the dark does not touch its colour.
        auto world = paint.Camera(view);
        paint.RoundRect(FloatRect::FromCenter(Sign, {180.0f, 60.0f}), 12.0f,
                        {.stroke = {5.0f, Neon}, .emission = 1.0f});
    }
    if (_guides) DrawLightGuides(paint, frame, view);
    // The HUD comes last, in canvas coordinates: light does not reach it.
}

RecipeLitScene::RecipeLitScene(const SceneServices& services, bool guides)
    : _view({static_cast<float>(services.canvas.x), static_cast<float>(services.canvas.y)}) {
    _view.SetGuides(guides);
}

void RecipeLitScene::HandleInput(const DeviceInput& input) {
    if (input.cancelled) {
        _move = {};
        return;
    }
    const auto axis = [&input](InputKey negative, InputKey positive) {
        return (input.Down(positive) ? 1.0f : 0.0f) - (input.Down(negative) ? 1.0f : 0.0f);
    };
    _move = {axis(InputKey::A, InputKey::D), axis(InputKey::W, InputKey::S)};
    if (input.mousePosition)
        _mouse = {static_cast<float>(input.mousePosition->x), static_cast<float>(input.mousePosition->y)};
}

std::string RecipeLitScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    if (id == "guides") _view.SetGuides(std::get<bool>(value));
    return {};
}

void RecipeLitScene::Update(float seconds) { _view.Update(seconds, _move, _mouse); }

void RecipeLitScene::Draw(Painter& paint) const { _view.Draw(paint); }

SceneDefinition RecipeLitSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.lighting";
    definition.name = "Lit room";
    definition.category = "Recipes";
    definition.parameters = {{"guides", "Light guides", SceneParameterKind::Boolean, false}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipeLitScene>(services, std::get<bool>(parameters.at("guides")));
    };
    return definition;
}
// [/recipe]
