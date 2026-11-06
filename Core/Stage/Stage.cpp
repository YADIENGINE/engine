#include "Stage/Stage.h"
#include <array>
#include <stdexcept>
#include "Audio/SoundPlayer.h"
#include "Camera/Debug/CameraGuides.h"
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Scene/SceneServices.h"

namespace {
FloatRect CanvasOf(const SceneServices& services) { return FloatRect::FromCorners({}, ToFloat2(services.canvas)); }

// A view hears what it shows: its middle is the listener, and a sound at its side edge is panned fully.
SoundListener ListenerOf(const CameraView& view) {
    SoundListener listener{.position = view.target, .rotation = view.rotation};
    const float width = view.VisibleArea().width;
    if (width > 0.0f) listener.panWidth = width * 0.5f;
    return listener;
}
} // namespace

Stage::Stage(const SceneServices& services, const StageSettings& settings)
    : _sounds(services.sounds)
    , _effects(services.effects)
    , _applied(services.effects)
    , _world(settings.camera.limits)
    , _listeners(settings.listeners)
    , _cameras(CanvasOf(services), settings.camera)
    , _fx(settings.camera.limits.value_or(CanvasOf(services)), CanvasOf(services), CurrentTheme().fx) {
    _fx.SetQuality(_applied);
    _fx.SetTime(&services.time);
    if (settings.atmosphere) _air.emplace(::Atmosphere(*settings.atmosphere), SecondsTween<::Weather>{});
    if (settings.light) _light.emplace(LightLayer(*settings.light));
}

::Atmosphere& Stage::Atmosphere() {
    if (!_air) throw std::logic_error("This stage has no atmosphere: give StageSettings::atmosphere");
    return _air->atmosphere;
}
SecondsTween<::Weather>& Stage::Weather() {
    if (!_air) throw std::logic_error("This stage has no atmosphere: give StageSettings::atmosphere");
    return _air->weather;
}
const SecondsTween<::Weather>& Stage::Weather() const {
    if (!_air) throw std::logic_error("This stage has no atmosphere: give StageSettings::atmosphere");
    return _air->weather;
}
LightLayer& Stage::Light() {
    if (!_light) throw std::logic_error("This stage has no light: give StageSettings::light");
    return _light->layer;
}
LightOccluders& Stage::Occluders() {
    if (!_light) throw std::logic_error("This stage has no light: give StageSettings::light");
    return _light->still;
}

void Stage::UpdateReal(float realSeconds) {
    if (!(_applied == _effects)) {
        _applied = _effects;
        _fx.SetQuality(_applied);
    }
    _fx.UpdateReal(realSeconds);
    Listen();
}
void Stage::Update(float gameSeconds) {
    _cameras.Update(gameSeconds);
    if (_air) {
        _air->weather.Update(gameSeconds);
        _air->atmosphere.Update(gameSeconds, _air->weather.Value());
    }
    _fx.Update(gameSeconds);
    Listen(); // The cameras moved
}
void Stage::SetViews(std::span<const CameraView> views) { _given.assign(views.begin(), views.end()); }
void Stage::Listen() {
    if (!_listeners) return;
    std::vector<SoundListener> listeners;
    if (_given.empty()) {
        for (const Camera2D& camera : _cameras)
            listeners.push_back(ListenerOf(camera.View()));
    } else {
        for (const CameraView& view : _given)
            listeners.push_back(ListenerOf(view));
    }
    _sounds.SetListeners(listeners);
}

void Stage::Draw(Painter& paint, const StageView& view) const {
    if (_light) {
        StageLight& frame = _light->frame;
        frame.ambient = {255, 255, 255, 255};
        frame.lights.clear();
        frame.moving.Clear();
        view.DescribeLight(paint, frame);
    }
    if (_given.empty()) {
        std::size_t index = 0;
        for (const Camera2D& camera : _cameras)
            DrawView(paint, view, camera.View(), camera.Settings().limits, index++, &camera);
    } else {
        for (std::size_t index = 0; index < _given.size(); ++index)
            DrawView(paint, view, _given[index], _world, index, nullptr);
    }
    _fx.DrawOverlay(paint);
    view.DrawHud(paint);
}

void Stage::DrawView(Painter& paint, const StageView& view, const CameraView& base,
                     const std::optional<FloatRect>& limits, std::size_t index, const Camera2D* camera) const {
    const StagePass pass{.view = _fx.WorldCamera(paint, base, limits), .camera = index};
    view.DrawBackground(paint, pass);
    {
        auto world = paint.Camera(pass.view);
        view.DrawFloor(paint, pass);
        _fx.DrawGroundLayer(paint);
    }
    if (_air) _air->atmosphere.DrawBehind(paint, pass.view);
    {
        auto world = paint.Camera(pass.view);
        view.DrawWorld(paint, pass);
    }
    view.DrawAir(paint, pass);
    if (_air) _air->atmosphere.DrawFront(paint, pass.view);
    std::array<const LightOccluders*, 2> occluders{};
    LightFrame lit;
    if (_light) {
        occluders = {&_light->still, &_light->frame.moving};
        lit = {.ambient = _light->frame.ambient,
               .lights = _light->frame.lights,
               .occluders = occluders,
               .flashes = _fx.LightFlashes()};
        _light->layer.Draw(paint, lit, pass.view);
    }
    {
        auto world = paint.Camera(pass.view);
        view.DrawGlow(paint, pass);
        _fx.DrawWorldLayer(paint);
    }
    if (_light && _guides.lights) DrawLightGuides(paint, lit, pass.view);
    if (_guides.cameras && camera) DrawCameraGuides(paint, *camera);
}
