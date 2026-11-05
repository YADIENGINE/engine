# Lighting recipe

A dark room with a lantern, a flashlight that follows the mouse, a flickering torch, shadows from a crate and from the player, and a neon sign that shines by itself.

APIs: Core/Graphics/Light/LightLayer.h, Core/Graphics/Light/LightOccluders.h, Core/Graphics/Light/CollisionOccluders.h, Core/Graphics/Light/Debug/LightGuides.h, Core/Graphics/Paint/PaintStyle.h (`LightSource`, `LightCone`). Concepts: [Lighting](../../Docs/Graphics/Lighting.md).

The order in `Draw` is the whole recipe: the world, the layer, what shines by itself, the HUD.

<!-- recipe:Lighting-header -->
```cpp
#pragma once
#include <string>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/SceneRegistry.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Light/LightOccluders.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Utilities/Geometry/Float2.h"

// A dark room: the player carries a lantern and aims a flashlight, a torch flickers on the wall, walls cast shadows, and
// a neon sign shines by itself. Lights are view code on seconds and decide nothing.
class RecipeLitView {
public:
    explicit RecipeLitView(Float2 canvas);
    void SetGuides(bool shown) { _guides = shown; }
    // Each frame: how the player moves (-1..1 per axis) and the mouse on the canvas, which the flashlight follows.
    void Update(float seconds, Float2 move, Float2 mouse);
    void Draw(Painter& paint) const;

private:
    [[nodiscard]] CameraView View() const;
    void DrawWorld(Painter& paint) const;

    Float2 _canvas;
    Float2 _player;
    Float2 _aim;
    float _time = 0.0f;
    bool _guides = false;
    LightLayer _lights; // Keeps only an image's memory: the lights are described in Draw, every frame
    CollisionWorld _colliders; // In a game: the simulation's, read by the view
    LightOccluders _walls;     // Built once from the colliders: walls stand still
};

class RecipeLitScene final : public EditorScene {
public:
    RecipeLitScene(const SceneServices& services, bool guides);

private:
    void HandleInput(const DeviceInput& input) override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    RecipeLitView _view;
    Float2 _move;
    Float2 _mouse;
};

[[nodiscard]] SceneDefinition RecipeLitSceneDefinition();
```
<!-- /recipe -->

<!-- recipe:Lighting -->
```cpp
#include "AgentDocs/Recipes/Code/Lighting.h"
#include <array>
#include <cmath>
#include <memory>
#include "Graphics/Light/CollisionOccluders.h"
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"
#include "Physics/Collision/Shape.h"

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
constexpr Float2 CrateAt{1000.0f, 600.0f};
constexpr Float2 CrateHalf{60.0f, 60.0f};
constexpr float PlayerSpeed = 320.0f;
constexpr float PlayerRadius = 18.0f;
enum class Layer { Wall };
} // namespace

RecipeLitView::RecipeLitView(Float2 canvas)
    : _canvas(canvas)
    , _player(Room.Center())
    , _aim(Room.Center() + Float2{100.0f, 0.0f}) {
    (void)_colliders.Add(Shape::Box(CrateHalf.x, CrateHalf.y), {CrateAt}, MaskOf(Layer::Wall));
    // Here light stops where the player stops. Anything else, such as a sprite's outline, goes in LightOccluders by hand.
    AddOccluders(_walls, _colliders, MaskOf(Layer::Wall));
}

void RecipeLitView::Update(float seconds, Float2 move, Float2 mouse) {
    _time += seconds;
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
    // What moves gives its outline from where it is drawn, so its shadow does not lag behind it. The player's own
    // lights stand inside this outline, so it stops only the torch.
    LightOccluders moving;
    const std::array<Float2, 8> body{Float2{1.0f, 0.0f}, {0.7f, 0.7f},  {0.0f, 1.0f},   {-0.7f, 0.7f},
                                     {-1.0f, 0.0f},      {-0.7f, -0.7f}, {0.0f, -1.0f}, {0.7f, -0.7f}};
    moving.Outline(body, Affine2::Translate(_player) * Affine2::Scale(PlayerRadius));
    const std::array<const LightOccluders*, 2> occluders{&_walls, &moving};

    const LightFrame frame{.ambient = Night, .lights = lights, .occluders = occluders};
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
```
<!-- /recipe -->

Look at it: `python Engine ctl open myproject.lighting --set guides=true --screenshot`; `guides` shows where each light stands, how far it reaches and what stops it.

Pitfalls: Call `LightLayer::Draw` outside `paint.Camera`, and give it the view the world was drawn through, shake included (`FxStage::WorldCamera`), or the lights slide off the world. Draw emissive shapes after the layer: before it their bloom stays but their colour goes dark. Move a light and a moving outline with the position the view draws (`StepSmoothed::At(alpha)`); a collider stands at the last step, so a shadow from `AddOccluders` lags on anything that moves. Do not add a collision layer only for light: layers are part of a simulation's state, so a look would change the replay hash. Lighting decides nothing: "can the guard see the player" belongs in the simulation, asked with `CollisionWorld::Raycast`. Use `Painter::Glow` for decoration that a low quality setting may remove, and a `LightSource` for anything the player needs to see.
