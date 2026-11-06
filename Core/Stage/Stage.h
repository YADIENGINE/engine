#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
#include "Animation/Tween.h"
#include "Camera/Cameras.h"
#include "Fx/FxQuality.h"
#include "Fx/FxStage.h"
#include "Graphics/Atmosphere/Atmosphere.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Light/LightOccluders.h"
#include "Stage/StageSettings.h"
#include "Stage/StageView.h"
#include "Weather/Weather.h"

class Painter;
class SoundPlayer;
struct SceneServices;

// Debug drawing over each view, after its effects.
struct StageGuides {
    bool cameras = false; // How each of the stage's own cameras follows (DrawCameraGuides)
    bool lights = false;  // Where the lights stand and what stops them (DrawLightGuides)
};

// A scene's view side in one place: its cameras, effects, optional light and weather, and the sound listeners, drawn in
// one fixed order. The scene says what is there through a StageView; it never writes the order of the passes.
//
//     class MyScene final : public Scene, StageView {
//     public:
//         explicit MyScene(const SceneServices& services) : _stage(services, {.light = LightLayerOptions{}}) {}
//     private:
//         void UpdateReal(float seconds) override { _stage.UpdateReal(seconds); } // Even in a hitstop or a pause
//         void Update(float seconds) override {
//             _stage.Cameras().Main().Follow(_player); // What the cameras keep in view
//             _stage.Update(seconds);                  // Game time
//         }
//         void Draw(Painter& paint) const override { _stage.Draw(paint, *this); }
//         void DrawWorld(Painter& paint, const StagePass&) const override { paint.Circle(_player, 20.0f, Fill(Hero)); }
//         Stage _stage;
//         Float2 _player;
//     };
//
// Every part stays usable on its own (Cameras, FxStage, LightLayer, Atmosphere); a Stage only assembles them. It is
// view code: it decides nothing, and a simulation cannot include it.
class Stage {
public:
    // Borrows the services' sound player, time scale (for cues that change time) and effect choice; they outlive the
    // scene. The effects play in the world (StageSettings::camera's limits, else the canvas) in the theme's style.
    // Throws std::invalid_argument for camera settings that are not IsValid.
    explicit Stage(const SceneServices& services, const StageSettings& settings = {});

    // The stage's own cameras: Main() for one view, Split for several. Follow, then Update the stage.
    [[nodiscard]] ::Cameras& Cameras() { return _cameras; }
    [[nodiscard]] const ::Cameras& Cameras() const { return _cameras; }
    // Plays effects: Fx().Play("explosion", {.at = where}). Positions are world positions.
    [[nodiscard]] FxStage& Fx() { return _fx; }
    [[nodiscard]] const FxStage& Fx() const { return _fx; }
    // The weather on screen; assign a new Atmosphere to change its look. Throws std::logic_error without
    // StageSettings::atmosphere.
    [[nodiscard]] ::Atmosphere& Atmosphere();
    // How much weather it shows: To() changes it over game time, Snap() sets it (each frame, for a simulation's
    // weather). Throws std::logic_error without StageSettings::atmosphere.
    [[nodiscard]] SecondsTween<::Weather>& Weather();
    [[nodiscard]] const SecondsTween<::Weather>& Weather() const;
    // The light layer; assign a new LightLayer to change its options. Throws std::logic_error without
    // StageSettings::light.
    [[nodiscard]] LightLayer& Light();
    // What stands still and stops light (walls, pillars): fill it once, AddOccluders for colliders. What moves comes
    // from StageView::DescribeLight every frame. Throws std::logic_error without StageSettings::light.
    [[nodiscard]] LightOccluders& Occluders();
    void SetGuides(const StageGuides& guides) { _guides = guides; }

    // Each frame from the scene's UpdateReal, which runs in a hitstop and a pause too: banners, toasts and how long
    // slow motion looks slow, a changed effect choice, and the listeners on the views.
    void UpdateReal(float realSeconds);
    // Each frame from the scene's Update, after the cameras were told what to follow: the cameras, the weather and the
    // effects' lifetimes and cue delays, on game time. A scene's Update does not run while game time stands still, so
    // a hitstop holds them. Each clock moves only its own parts: a scene whose two hooks both get real seconds
    // (TimeDomain::Real) moves nothing twice.
    void Update(float gameSeconds);
    // Views given from outside, such as a camera that lives in a simulation and is drawn between its steps
    // (Camera2D::View(runner.Alpha())): drawn and heard instead of the own cameras until the next SetViews. Copied. An
    // empty span returns to the own cameras. Callable at any time, the constructor included.
    void SetViews(std::span<const CameraView> views);

    // The frame. For each view: background, floor, ground effects, atmosphere behind, world, air, atmosphere in front,
    // light, glow, world effects and guides. Then once over the canvas: the effects' overlay and the HUD. Call it
    // outside paint.Camera and paint.Transform.
    void Draw(Painter& paint, const StageView& view) const;

private:
    struct Air {
        ::Atmosphere atmosphere;
        SecondsTween<::Weather> weather;
    };
    struct Lighting {
        LightLayer layer;
        LightOccluders still;
        mutable StageLight frame; // Refilled by DescribeLight in each Draw
    };
    void Listen();
    void DrawView(Painter& paint, const StageView& view, const CameraView& base, const std::optional<FloatRect>& limits,
                  std::size_t index, const Camera2D* camera) const;

    SoundPlayer& _sounds;
    const FxQuality& _effects;
    FxQuality _applied;
    std::optional<FloatRect> _world;
    bool _listeners;
    ::Cameras _cameras;
    FxStage _fx;
    std::optional<Air> _air;
    std::optional<Lighting> _light;
    std::vector<CameraView> _given;
    StageGuides _guides;

public:
    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;
};
