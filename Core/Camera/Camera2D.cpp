#include "Camera/Camera2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Animation/SmoothDamp.h"
#include "Camera/Internals/CameraSolver.h"

namespace {
const CameraSettings& Checked(const CameraSettings& settings) {
    if (!IsValid(settings)) throw std::invalid_argument("Invalid camera settings");
    return settings;
}
Float2 ClampEach(Float2 value, Float2 limit) {
    return {std::clamp(value.x, -limit.x, limit.x), std::clamp(value.y, -limit.y, limit.y)};
}
} // namespace

Camera2D::Camera2D(const FloatRect& viewport, const CameraSettings& settings)
    : _viewport(viewport)
    , _settings(Checked(settings))
    , _pose{.center = viewport.Center(), .zoom = settings.zoom.zoom} // At zoom 1 the world is the canvas
    , _previous(_pose)
    , _goal(_pose.center)
    , _focus(_pose.center) {
    KeepInLimits();
    _previous = _pose;
}
void Camera2D::SetViewport(const FloatRect& viewport) {
    _viewport = viewport;
    KeepInLimits();
}
void Camera2D::SetSettings(const CameraSettings& settings) { _settings = Checked(settings); }
void Camera2D::SetLimits(std::optional<FloatRect> limits) {
    CameraSettings settings = _settings;
    settings.limits = limits;
    SetSettings(settings);
}
void Camera2D::Follow(Float2 target) { Follow({CameraTarget{.position = target}}); }
void Camera2D::Follow(std::span<const CameraTarget> targets) { _targets.assign(targets.begin(), targets.end()); }
void Camera2D::Follow(std::initializer_list<CameraTarget> targets) {
    Follow(std::span<const CameraTarget>(targets.begin(), targets.size()));
}
void Camera2D::StopFollowing() {
    _targets.clear();
    _tracking = false;
}
void Camera2D::Update(float seconds) {
    _previous = _pose;
    if (seconds > 0.0f) Solve(seconds, false);
}
void Camera2D::Snap() {
    Solve(0.0f, true);
    _previous = _pose;
}
void Camera2D::SetPose(const CameraPose& pose) {
    _previous = _pose;
    _pose = pose;
    _velocity = {};
    _zoomVelocity = 0.0f;
    KeepInLimits();
    _goal = _pose.center;
}
CameraView Camera2D::View(float alpha) const {
    return ViewOf(MixPose(_previous, _pose, std::clamp(alpha, 0.0f, 1.0f)));
}
bool Camera2D::Shows(Float2 world, float radius) const {
    const FloatRect area = VisibleArea();
    return world.x + radius >= area.x && world.x - radius <= area.Right() && world.y + radius >= area.y &&
           world.y - radius <= area.Bottom();
}

// One update. 'jump' arrives at once and forgets motion: Snap.
void Camera2D::Solve(float seconds, bool jump) {
    const CameraGroup group = GatherTargets(_targets);
    if (!group.any) {
        _tracking = false;
        KeepInLimits();
        return;
    }
    const auto& follow = _settings.follow;
    const auto& zoom = _settings.zoom;
    const float smooth = jump ? 0.0f : follow.smoothTime;

    const float zoomGoal = zoom.fit ? FitZoom(group.bounds, _viewport, zoom) : zoom.zoom;
    _pose.zoom = jump ? zoomGoal : SmoothDamp(_pose.zoom, zoomGoal, _zoomVelocity, zoom.smoothTime, seconds);
    if (jump) _zoomVelocity = 0.0f;
    const Float2 visible = VisibleSize(_viewport, _pose.zoom);

    // Look ahead along the targets' motion, measured between updates.
    const Float2 motion = _tracking && !jump ? (group.center - _lastCenter) / seconds : Float2{};
    const Float2 ahead = ClampEach({motion.x * follow.lookAhead.x, motion.y * follow.lookAhead.y},
                                   visible * follow.lookAheadLimit);
    if (jump) {
        _lookAhead = {};
        _lookAheadVelocity = {};
    } else {
        _lookAhead = SmoothDamp(_lookAhead, ahead, _lookAheadVelocity, follow.lookAheadSmoothTime, seconds);
    }
    _lastCenter = group.center;
    _tracking = true;

    _focus = group.center + follow.offset + _lookAhead;
    _goal = jump ? _focus : ApplyDeadZone(_goal, _focus, {visible.x * follow.deadZone.x * 0.5f,
                                                          visible.y * follow.deadZone.y * 0.5f});
    if (zoom.fit) _goal = KeepInView(_goal, group.bounds, visible, zoom.padding / _pose.zoom);
    if (_settings.limits) _goal = Confine(_goal, visible, *_settings.limits);

    if (!(smooth > 0.0f)) {
        _pose.center = _goal;
        _velocity = {};
    } else {
        _pose.center = SmoothDamp(_pose.center, _goal, _velocity, smooth, seconds);
    }
    KeepInLimits();
}
void Camera2D::KeepInLimits() {
    if (_settings.limits) _pose.center = Confine(_pose.center, VisibleSize(_viewport, _pose.zoom), *_settings.limits);
}
CameraView Camera2D::ViewOf(const CameraPose& pose) const {
    CameraView view{.offset = _viewport.Center(), .target = pose.center, .rotation = pose.rotation, .zoom = pose.zoom,
                    .viewport = _viewport};
    if (_settings.pixelSnap) view.target = SnapToPixels(view.target, view.offset, view.zoom);
    return view;
}
