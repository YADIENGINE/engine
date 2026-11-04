#include "Tests/Simulation/ToyTimeline.h"
#include <array>
#include <memory>
#include "Animation/Easing.h"
#include "Simulation/StateVisitor.h"
#include "Simulation/TimelineMoments.h"

namespace {
constexpr std::array Curves{
    EaseLinear,    EaseInQuad,    EaseOutQuad,    EaseInOutQuad,    EaseInCubic,  EaseOutCubic,
    EaseInOutCubic, EaseInQuart,  EaseOutQuart,   EaseInOutQuart,   EaseInSine,   EaseOutSine,
    EaseInOutSine, EaseInExpo,    EaseOutExpo,    EaseInBack,       EaseOutBack,  EaseInOutBack,
    EaseInElastic, EaseOutElastic, EaseInBounce,  EaseOutBounce,
};
} // namespace

ToyTimelineSimulation::ToyTimelineSimulation(const SimulationStart&) {}
void ToyTimelineSimulation::Step(const SimulationStep&) {
    _stage.ForEachCrossed(_age, _age + 1, [&](std::string_view moment) {
        if (moment == "wave" || moment == "secondWave") ++_waves;
        if (moment == "boss") _attackStart = _age;
    });
    if (_attackStart >= 0) {
        const int cycle = _attack.Wrap(_age - _attackStart);
        _attack.ForEachCrossed(cycle, cycle + 1, [&](std::string_view moment) {
            if (moment == "fire") ++_shots;
        });
        _charge = _attack.Progress("windup", "fire", cycle);
    }
    const float along = _stage.Progress("intro", "end", _age);
    for (const auto curve : Curves)
        _curves += static_cast<double>(curve(along));
    ++_age;
}
void ToyTimelineSimulation::State(StateVisitor& state) const {
    state.Field("age", _age);
    state.Field("phase", _stage.PhaseAt(_age));
    state.Field("attackStart", _attackStart);
    state.Field("waves", _waves);
    state.Field("shots", _shots);
    state.Field("charge", _charge);
    state.Field("curves", _curves);
}
std::vector<SimulationMoment> ToyTimelineSimulation::Moments() const {
    std::vector<SimulationMoment> moments;
    AppendMoments(moments, _stage);
    // The attack's steps are known only once the boss has come in.
    if (_attackStart >= 0) AppendMoments(moments, _attack, static_cast<std::uint64_t>(_attackStart), "attack.");
    return moments;
}
SimulationDefinition ToyTimelineDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "engine.toy-timeline";
    definition.create = [](const SimulationStart& start) { return std::make_unique<ToyTimelineSimulation>(start); };
    return definition;
}
