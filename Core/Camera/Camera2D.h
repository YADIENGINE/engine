#pragma once
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>
#include "Animation/Pose.h"
#include "Camera/CameraPose.h"
#include "Camera/CameraSettings.h"
#include "Camera/CameraTarget.h"
#include "Camera/CameraView.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

// A 2D game camera, in the manner of Godot's Camera2D and Cinemachine's follow camera: it follows one or more
// targets with a dead zone, look-ahead and smoothing, zooms to fit several, stays inside limits, and gives the view
// the world is drawn through.
//
//     Camera2D camera(canvas, {.follow = {.deadZone = {0.2f, 0.3f}, .lookAhead = {0.4f, 0.0f}}, .limits = level});
//     camera.Follow(player.At(alpha));               // Each frame: where the targets are now
//     camera.Update(seconds);                        // Then move
//     auto world = paint.Camera(camera.View());      // Draw the world through it
//
// A view on seconds is the usual home. When the camera decides gameplay (enemies wake as they come into view, the
// screen never scrolls back), keep it in the simulation instead: Update(step.seconds) every step, name it in State, and
// draw with View(runner.Alpha()). Its math is the same and deterministic there. Cameras holds one per player for split
// screen, with Main() first.
class Camera2D {
public:
    // Throws std::invalid_argument for settings that are not IsValid.
    explicit Camera2D(const FloatRect& viewport, const CameraSettings& settings = {});

    // Where on the canvas it draws. The world under the middle stays put.
    void SetViewport(const FloatRect& viewport);
    [[nodiscard]] const FloatRect& Viewport() const { return _viewport; }
    // Throws std::invalid_argument for settings that are not IsValid. Takes effect from the next Update; a camera with
    // no targets keeps its pose.
    void SetSettings(const CameraSettings& settings);
    [[nodiscard]] const CameraSettings& Settings() const { return _settings; }
    void SetLimits(std::optional<FloatRect> limits);

    // What to keep in view from the next Update on, until changed: a point, or several with weights and radii. Copied,
    // so call it with the current positions before each Update.
    void Follow(Float2 target);
    void Follow(std::span<const CameraTarget> targets);
    void Follow(std::initializer_list<CameraTarget> targets);
    // Stays where it is; SetPose moves it by hand.
    void StopFollowing();
    [[nodiscard]] std::span<const CameraTarget> Targets() const { return _targets; }

    // Moves towards the targets: frame seconds in a view (game time, so a hitstop holds it), step.seconds in a
    // simulation. Without targets it stays, inside its limits.
    void Update(float seconds);
    // Jumps to where it is heading, with no smoothing or look-ahead left over: a level's start, a teleport, a cut.
    void Snap();

    // Places it by hand, for a cutscene or a tweened CameraPose; limits still apply. Call it or Update once per frame
    // (or step), not both: each makes the pose before it the one View(alpha) starts from.
    void SetPose(const CameraPose& pose);
    [[nodiscard]] const CameraPose& Pose() const { return _pose; }

    // What it shows now.
    [[nodiscard]] CameraView View() const { return ViewOf(_pose); }
    // Between the last two updates by 'alpha' (0..1): a camera updated in a simulation's steps, drawn at
    // runner.Alpha().
    [[nodiscard]] CameraView View(float alpha) const;
    // The world rectangle it shows, ignoring rotation.
    [[nodiscard]] FloatRect VisibleArea() const { return View().VisibleArea(); }
    // A canvas point (the mouse) in the world, and back.
    [[nodiscard]] Float2 ToWorld(Float2 canvas) const { return View().ToWorld(canvas); }
    [[nodiscard]] Float2 ToCanvas(Float2 world) const { return View().ToScreen(world); }
    [[nodiscard]] bool Shows(Float2 world, float radius = 0.0f) const;

    // What the follow is aiming at, for guides and tests: the targets' point with offset and look-ahead, and where the
    // camera is heading after the dead zone, fitting and limits.
    [[nodiscard]] Float2 Focus() const { return _focus; }
    [[nodiscard]] Float2 Goal() const { return _goal; }

    // Everything that decides later updates, for a simulation's State. Settings are data: change them only from state
    // the simulation already names.
    template <class Visitor> void State(Visitor& state) const {
        VisitPose(state, _pose);
        state.Field("goal", _goal);
        state.Field("focus", _focus);
        state.Field("velocity", _velocity);
        state.Field("zoomVelocity", _zoomVelocity);
        state.Field("lookAhead", _lookAhead);
        state.Field("lookAheadVelocity", _lookAheadVelocity);
        state.Field("lastCenter", _lastCenter);
        state.Field("tracking", _tracking);
        for (std::size_t i = 0; i < _targets.size(); ++i) {
            state.Group("target", i, [&] {
                state.Field("position", _targets[i].position);
                state.Field("weight", _targets[i].weight);
                state.Field("radius", _targets[i].radius);
            });
        }
    }

private:
    void Solve(float seconds, bool jump);
    void KeepInLimits();
    [[nodiscard]] CameraView ViewOf(const CameraPose& pose) const;
    FloatRect _viewport;
    CameraSettings _settings;
    std::vector<CameraTarget> _targets;
    CameraPose _pose;
    CameraPose _previous;
    Float2 _goal{};
    Float2 _focus{};
    Float2 _velocity{};
    float _zoomVelocity = 0.0f;
    Float2 _lookAhead{};
    Float2 _lookAheadVelocity{};
    Float2 _lastCenter{};
    bool _tracking = false; // _lastCenter holds the targets' last point, so their speed is known
};
