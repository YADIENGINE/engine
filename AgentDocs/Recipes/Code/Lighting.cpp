// [recipe:Lighting]
#include "AgentDocs/Recipes/Code/Lighting.h"
#include <array>
#include <cmath>
#include <memory>
#include "Animation/Flicker.h"
#include "Fx/FxCue.h"
#include "Graphics/Light/CollisionOccluders.h"
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Input/Input.h"
#include "Physics/Collision/Shape.h"

namespace {
constexpr Color4 Floor = Rgb(0x8a8378);
constexpr Color4 Crate = Rgb(0xb5793a);
constexpr Color4 Neon = Rgb(0x30e0ff);
constexpr Color4 Night = Rgb(0x10121c);
constexpr Color4 LanternColor = Rgb(0xffd9a0);
constexpr Color4 TorchColor = Rgb(0xff8a3c);
constexpr Color4 LightningColor = Rgb(0xb8ccff);
constexpr unsigned TorchSeed = 1U;
constexpr FloatRect Room{0.0f, 0.0f, 1600.0f, 1000.0f};
constexpr Float2 Torch{1100.0f, 300.0f};
constexpr Float2 Sign{500.0f, 280.0f};
constexpr Float2 CrateAt{1000.0f, 600.0f};
constexpr Float2 CrateHalf{60.0f, 60.0f};
constexpr float PlayerSpeed = 320.0f;
constexpr float PlayerRadius = 18.0f;
enum class Layer { Wall };
} // namespace

RecipeLitView::RecipeLitView(Float2 canvas)
    : _canvas(canvas)
    , _player(Room.Center())
    , _aim(Room.Center() + Float2{100.0f, 0.0f})
    , _fx(FloatRect{0.0f, 0.0f, canvas.x, canvas.y}, FloatRect{0.0f, 0.0f, canvas.x, canvas.y}, CurrentTheme().fx) {
    (void)_colliders.Add(Shape::Box(CrateHalf.x, CrateHalf.y), {CrateAt}, MaskOf(Layer::Wall));
    // Here light stops where the player stops. Anything else, such as a sprite's outline, goes in LightOccluders by hand.
    AddOccluders(_walls, _colliders, MaskOf(Layer::Wall));
}

void RecipeLitView::SetEffects(const FxQuality& effects) {
    _flashes = effects.flashes;
    _fx.SetQuality(effects);
}

void RecipeLitView::Lightning() {
    // No radius: the whole scene, for a moment. Usually one line of a named cue.
    _fx.Play(FxCue{FxLightFlash{.seconds = 0.45f, .strength = 0.8f}}, {.color = LightningColor});
}

void RecipeLitView::Update(float seconds, Float2 move, Float2 mouse) {
    _time += seconds;
    _fx.Update(seconds, seconds);
    _player += move * (PlayerSpeed * seconds);
    _aim = View().ToWorld(mouse);
}

CameraView RecipeLitView::View() const { return {.offset = _canvas * 0.5f, .target = _player}; }

void RecipeLitView::DrawWorld(Painter& paint) const {
    paint.Rect(Room, Fill(Floor));
    paint.Rect(FloatRect::FromCenter(CrateAt, CrateHalf * 2.0f), Fill(Crate));
    paint.Circle(_player, PlayerRadius, Fill(Rgb(0xe8ecff)));
}

void RecipeLitView::Draw(Painter& paint) const {
    const CameraView view = View();
    {
        auto world = paint.Camera(view);
        DrawWorld(paint);
    }

    const Float2 aimed = _aim - _player;
    const std::array lights{
        LightSource{.at = _player, .radius = 160.0f, .color = LanternColor, .intensity = 0.7f},
        LightSource{.at = _player,
                    .radius = 520.0f,
                    .cone = {.direction = std::atan2(aimed.y, aimed.x), .halfAngle = 0.35f}},
        // A light's strength can be any function of time. Flicker needs the player's choice, so it cannot be forgotten.
        LightSource{.at = Torch,
                    .radius = 300.0f,
                    .color = TorchColor,
                    .intensity = Flicker(_time, FlickerStyle::Candle(), TorchSeed, _flashes),
                    .core = 0.15f},
    };
    // What moves gives its outline from where it is drawn, so its shadow does not lag behind it. The player's own
    // lights stand inside this outline, so it stops only the torch.
    LightOccluders moving;
    const std::array<Float2, 8> body{Float2{1.0f, 0.0f}, {0.7f, 0.7f},  {0.0f, 1.0f},   {-0.7f, 0.7f},
                                     {-1.0f, 0.0f},      {-0.7f, -0.7f}, {0.0f, -1.0f}, {0.7f, -0.7f}};
    moving.Outline(body, Affine2::Translate(_player) * Affine2::Scale(PlayerRadius));
    const std::array<const LightOccluders*, 2> occluders{&_walls, &moving};

    const LightFrame frame{.ambient = Night, .lights = lights, .occluders = occluders, .flashes = _fx.LightFlashes()};
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
    const auto axis = [&input](InputKey negative, InputKey positive) {
        return (input.Down(positive) ? 1.0f : 0.0f) - (input.Down(negative) ? 1.0f : 0.0f);
    };
    _move = {axis(InputKey::A, InputKey::D), axis(InputKey::W, InputKey::S)};
    _lightning = input.Pressed(InputKey::Space);
    if (input.mousePosition)
        _mouse = {static_cast<float>(input.mousePosition->x), static_cast<float>(input.mousePosition->y)};
}
void RecipeLitScene::InputCancelled() { _move = {}; }

std::string RecipeLitScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    if (id == "guides") _view.SetGuides(std::get<bool>(value));
    return {};
}

void RecipeLitScene::Update(float seconds) {
    if (_lightning) _view.Lightning();
    _lightning = false;
    _view.Update(seconds, _move, _mouse);
}

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
