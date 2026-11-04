#include "Tests/Simulation/ToyAnimation.h"
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include "Simulation/StateVisitor.h"

namespace {
constexpr int UnitCount = 6;
constexpr std::array<std::string_view, 4> ClipNames{"idle", "walk", "attack", "fall"};

StepClipSet<ToyPose> MakeClips() {
    return {
        {"idle", StepClip<ToyPose>{}
                     .Track(&ToyPose::arm, {{0, 0.0f}, {30, 0.4f, Curve::InOutSine}, {60, 0.0f, Curve::InOutSine}})
                     .Track(&ToyPose::scale, {{0, 1.0f}, {30, 1.1f, Curve::OutElastic}, {60, 1.0f, Curve::InElastic}})
                     .Looping()},
        {"walk", StepClip<ToyPose>{}
                     .Track(&ToyPose::offset, {{0, Float2{0.0f, 0.0f}},
                                               {10, Float2{4.0f, -3.0f}, Curve::OutExpo},
                                               {20, Float2{0.0f, 0.0f}, Curve::InExpo}})
                     .Track(&ToyPose::frame, {{0, 0}, {5, 1}, {10, 2}, {15, 3}})
                     .Moment("footstep", 10)
                     .Looping()},
        {"attack", StepClip<ToyPose>{}
                       .Track(&ToyPose::arm, {{0, 0.0f}, {12, -1.2f, Curve::InBack}, {18, 0.9f, Curve::OutBounce}, {30, 0.0f, Curve::InOutBack}})
                       .Track(&ToyPose::tint, {{0, Color4{255, 255, 255, 255}}, {18, Color4{255, 40, 20, 255}, Curve::InQuart}, {30, Color4{255, 255, 255, 255}, Curve::OutQuart}})
                       .Moment("fire", 18)
                       .Moment("done", 30)},
        {"fall", StepClip<ToyPose>{}
                     .Track(&ToyPose::offset, {{0, Float2{0.0f, 0.0f}}, {25, Float2{0.0f, 40.0f}, Curve::OutBounce}})
                     .Track(&ToyPose::scale, {{0, 1.0f}, {25, 0.7f, Curve::InBounce}})},
    };
}
double Sum(const ToyPose& pose) {
    return static_cast<double>(pose.arm) + static_cast<double>(pose.scale) + static_cast<double>(pose.offset.x) +
           static_cast<double>(pose.offset.y) + static_cast<double>(pose.tint.g) + static_cast<double>(pose.frame);
}
} // namespace

ToyAnimationSimulation::ToyAnimationSimulation(const SimulationStart& start)
    : _rng(start.seed) {
    const auto clips = MakeClips();
    for (int i = 0; i < UnitCount; ++i)
        _units.push_back({StepAnimator<ToyPose>(clips, "idle")});
}
void ToyAnimationSimulation::Step(const SimulationStep&) {
    for (std::size_t i = 0; i < _units.size(); ++i) {
        Unit& unit = _units[i];
        if (_rng.Float(0.0f, 1.0f) < 0.06f) {
            if (unit.animator.Blending()) ++_interrupted;
            const auto name = ClipNames[static_cast<std::size_t>(_rng.Float(0.0f, 3.999f))];
            unit.animator.Play(name, {.blend = static_cast<int>(_rng.Float(0.0f, 20.0f)),
                                      .blendCurve = Curve::InOutQuad,
                                      .then = name == "attack" ? "idle" : "",
                                      .thenBlend = 8});
        }
        if (unit.animator.Step([&](std::string_view moment) {
                if (moment == "fire") {
                    ++_shots;
                    _flash.To(Color4{255, 0, 0, 255}, 3, Curve::OutQuad).Then(Color4{0, 0, 0, 255}, 9, Curve::InSine);
                    _marks.To(_shots % 5, static_cast<float>(_shots % 3), 7, Curve::OutBack);
                }
            }))
            ++_finished;
        if (_rng.Float(0.0f, 1.0f) < 0.02f)
            unit.speed.To(_rng.Float(0.5f, 2.0f), 15, Curve::OutElastic).Wait(5).Then(1.0f, 10, Curve::InOutBack);
        unit.speed.Step();
        _shown += Sum(unit.animator.Pose()) * static_cast<double>(unit.speed.Value());
    }
    if (_age % 90 == 0) _camera.To(Float2{_rng.Float(-9.0f, 9.0f), _rng.Float(-9.0f, 9.0f)}, 30, Curve::InOutCubic);
    UpdateAll(1, _camera, _flash);
    _marks.Step([&](int) { _shown += 1.0; });
    _shown += static_cast<double>(_camera.Value().x) + static_cast<double>(_flash.Value().r) +
              static_cast<double>(_marks.Value(1));
    ++_age;
}
void ToyAnimationSimulation::State(StateVisitor& state) const {
    state.Field("age", _age);
    state.Field("rng", _rng);
    state.Field("shots", _shots);
    state.Field("finished", _finished);
    state.Field("interrupted", _interrupted);
    state.Field("shown", _shown);
    for (std::size_t i = 0; i < _units.size(); ++i) {
        state.Group("unit", i, [&] {
            state.Group("animator", [&] { _units[i].animator.State(state); });
            state.Group("speed", [&] { _units[i].speed.State(state); });
        });
    }
    state.Group("camera", [&] { _camera.State(state); });
    state.Group("flash", [&] { _flash.State(state); });
    state.Group("marks", [&] { _marks.State(state); });
}
SimulationDefinition ToyAnimationDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "engine.toy-animation";
    definition.create = [](const SimulationStart& start) { return std::make_unique<ToyAnimationSimulation>(start); };
    return definition;
}
