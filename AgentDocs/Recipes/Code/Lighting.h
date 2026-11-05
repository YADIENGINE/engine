// [recipe:Lighting-header]
#pragma once
#include <string>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/SceneRegistry.h"
#include "Fx/FxStage.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Light/LightOccluders.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Utilities/Geometry/Float2.h"

// A dark room: the player carries a lantern and aims a flashlight, a torch flickers on the wall, lightning lights it
// all for a moment, walls cast shadows, and a neon sign shines by itself. Lights are view code on seconds and decide
// nothing.
class RecipeLitView {
public:
    explicit RecipeLitView(Float2 canvas);
    void SetGuides(bool shown) { _guides = shown; }
    // The player's choice, Game::Effects(): the stage and the flicker both honour it.
    void SetEffects(const FxQuality& effects);
    void Lightning();
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
    bool _flashes = true;
    FxStage _fx;
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
    bool _lightning = false;
};

[[nodiscard]] SceneDefinition RecipeLitSceneDefinition();
// [/recipe]
