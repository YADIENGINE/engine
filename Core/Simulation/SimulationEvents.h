#pragma once
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include "Simulation/SimulationEventLog.h"
#include "Simulation/SimulationSettings.h"
#include "Simulation/SimulationStep.h"
#include "Simulation/StateVisitor.h"

// An event type: a plain value naming itself. Its fields are optional; tools show them (sim run, scenario checks):
//     struct Hit {
//         static constexpr std::string_view Name = "Hit";
//         Float2 at;
//         int enemy = 0;
//         void State(StateVisitor& state) const { state.Field("at", at); state.Field("enemy", enemy); }
//     };
template <class T>
concept SimulationEventType = std::copy_constructible<T> && requires {
    { T::Name } -> std::convertible_to<std::string_view>;
};
template <class T> inline constexpr bool IsSimulationEventSet = SimulationEventType<T>;
template <class... T> inline constexpr bool IsSimulationEventSet<std::variant<T...>> = (SimulationEventType<T> && ...);
// What SimulationEvents holds: one event type, or a game's std::variant of them (one list keeps their order).
template <class T>
concept SimulationEventSet = IsSimulationEventSet<T>;

// One event, as the simulation emitted it.
template <class E> struct StepEvent {
    std::uint64_t step = 0;     // The step that emitted it
    std::uint64_t sequence = 0; // Its number in the run, from 0, in the order of emission
    E event;
};

// What a simulation tells its view happened during a step: a hit, a death, a pickup. The view plays sounds and effects
// for them; the simulation itself never reads them back. Keep one as a member, emit during Step, and expose it:
//
//     using GameEvent = std::variant<Hit, Death>;
//     void Step(const SimulationStep& step) override { ... _events.Emit(step, Hit{at, enemy}); }
//     const SimulationEvents<GameEvent>& Events() const { return _events; }
//     const SimulationEventLog* EventLog() const override { return &_events; } // For tools
//
// Events are output, not state: leave them out of State and the hash. A replay emits the same ones because it runs the
// same steps. Each view reads them with its own SimulationEventReader, so a sound player and a score popup can both
// read every event. Events of the last SimulationMaxFrameSeconds of steps are kept, which covers everything one frame
// runs; older ones are dropped when a new one is emitted.
template <SimulationEventSet E> class SimulationEvents final : public SimulationEventLog {
public:
    void Emit(const SimulationStep& step, E event) {
        Forget(step);
        _events.push_back({step.index, _next++, std::move(event)});
    }
    // Every kept event, oldest first. Valid until the next Emit.
    [[nodiscard]] std::span<const StepEvent<E>> Kept() const {
        return std::span<const StepEvent<E>>(_events).subspan(_first);
    }
    // Kept events numbered 'sequence' or later.
    [[nodiscard]] std::span<const StepEvent<E>> Since(std::uint64_t sequence) const {
        const std::uint64_t first = FirstKept();
        const auto skip = sequence > first ? static_cast<std::size_t>(sequence - first) : std::size_t{0};
        const auto kept = Kept();
        return kept.subspan(skip < kept.size() ? skip : kept.size());
    }

    [[nodiscard]] std::uint64_t End() const override { return _next; }
    [[nodiscard]] std::uint64_t FirstKept() const override { return _next - (_events.size() - _first); }
    [[nodiscard]] std::uint64_t StepOf(std::uint64_t sequence) const override { return At(sequence).step; }
    [[nodiscard]] std::string_view NameOf(std::uint64_t sequence) const override {
        const E& event = At(sequence).event;
        if constexpr (SimulationEventType<E>) {
            return E::Name;
        } else {
            return std::visit(
                [](const auto& one) -> std::string_view { return std::remove_cvref_t<decltype(one)>::Name; }, event);
        }
    }
    void FieldsOf(std::uint64_t sequence, StateVisitor& fields) const override {
        const auto visit = [&fields](const auto& one) {
            if constexpr (requires { one.State(fields); }) one.State(fields);
        };
        const E& event = At(sequence).event;
        if constexpr (SimulationEventType<E>)
            visit(event);
        else
            std::visit(visit, event);
    }

private:
    // Keeps the steps one frame can run, and one more, up to 'step': at 60 steps a second, 16 steps.
    void Forget(const SimulationStep& step) {
        if (!(step.seconds > 0.0f)) return;
        const double stepsPerSecond = std::round(1.0 / static_cast<double>(step.seconds));
        const auto keep = static_cast<std::uint64_t>(std::ceil(SimulationMaxFrameSeconds * stepsPerSecond)) + 1U;
        if (step.index < keep) return;
        const std::uint64_t oldest = step.index - keep;
        while (_first < _events.size() && _events[_first].step <= oldest)
            ++_first;
        if (_first == _events.size()) {
            _events.clear();
            _first = 0;
        } else if (_first >= CompactAfter && _first * 2U >= _events.size()) {
            _events.erase(_events.begin(), _events.begin() + static_cast<std::ptrdiff_t>(_first));
            _first = 0;
        }
    }
    [[nodiscard]] const StepEvent<E>& At(std::uint64_t sequence) const {
        return _events[_first + static_cast<std::size_t>(sequence - FirstKept())];
    }
    static constexpr std::size_t CompactAfter = 64;
    std::vector<StepEvent<E>> _events;
    std::size_t _first = 0; // Events before it are forgotten
    std::uint64_t _next = 0;
};
