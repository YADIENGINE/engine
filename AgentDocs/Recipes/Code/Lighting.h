// [recipe:Lighting-header]
#pragma once
#include <string>
#include "Editor/Api/SceneRegistry.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Scene/Scene.h"
#include "Stage/Stage.h"
#include "Utilities/Geometry/Float2.h"

// A dark room: the player carries a lantern and aims a flashlight, a torch flickers on the wall, lightning lights it
// all for a moment, walls cast shadows, and a neon sign shines by itself. Lights are view code on seconds and decide
// nothing: the stage asks for them every frame (DescribeLight) and darkens the world before the glow pass.
class RecipeLitView final : public StageView {
public:
    explicit RecipeLitView(const SceneServices& services);
    void SetGuides(bool shown) { _stage.SetGuides({.lights = shown}); }
    void Lightning();
    // Each frame, from the scene's two hooks: real time, and game time with how the player moves (-1..1 per axis) and
    // the mouse, which the flashlight follows.
    void UpdateReal(float realSeconds) { _stage.UpdateReal(realSeconds); }
    void Update(float seconds, Float2 move, Float2 mouse);
    void Draw(Painter& paint) const { _stage.Draw(paint, *this); }

private:
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    void DrawGlow(Painter& paint, const StagePass& pass) const override;
    void DescribeLight(const Painter& paint, StageLight& light) const override;

    Stage _stage;
    Float2 _player;
    Float2 _aim;
    CollisionWorld _colliders; // In a game: the simulation's, read by the view
};

class RecipeLitScene final : public Scene {
public:
    RecipeLitScene(const SceneServices& services, bool guides);
    ~RecipeLitScene() override { ShutdownNoThrow(); }

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void UpdateReal(float realSeconds) override { _view.UpdateReal(realSeconds); }
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    RecipeLitView _view;
    Float2 _move;
    Float2 _mouse;
    bool _lightning = false;
};

[[nodiscard]] SceneDefinition RecipeLitSceneDefinition();
// [/recipe]
