#pragma once
#include <cstdint>
#include <span>
#include "Simulation/SimulationEvents.h"
#include "Simulation/SimulationRunner.h"

// One view's place in a simulation's events: each Read returns those it has not seen yet. Keep one per thing that
// reacts (sounds, effects, a score popup); they do not disturb each other. Read after the runner advances:
//
//     _runner.Advance(seconds);
//     for (const auto& happened : _reader.Read(_simulation.Events(), _runner))
//         std::visit(Overloaded{[&](const Hit& hit) { _fx.Play("hit", {.at = hit.at}); },
//                               [&](const Death&) { _sounds.Play(Sfx::Death); }},
//                    happened.event);
//
// A view on seconds holds no state the simulation needs, so a reader is never part of State.
template <SimulationEventSet E> class SimulationEventReader {
public:
    // Events not read yet, oldest first, without those of steps the runner was catching up on (an editor seek, RunTo):
    // 600 skipped steps must not play 600 sounds. Valid until the simulation's next step.
    [[nodiscard]] std::span<const StepEvent<E>> Read(const SimulationEvents<E>& events,
                                                     const SimulationRunner& runner) {
        auto unread = ReadAll(events);
        const std::uint64_t silent = runner.EventsSilencedUntil();
        std::size_t skip = 0;
        while (skip < unread.size() && unread[skip].step < silent)
            ++skip;
        return unread.subspan(skip);
    }
    // Every event not read yet, those of skipped steps included: for what must reach its final value whatever was
    // skipped, such as a score counter.
    [[nodiscard]] std::span<const StepEvent<E>> ReadAll(const SimulationEvents<E>& events) {
        const std::uint64_t first = events.FirstKept();
        if (_next < first) {
            _missed += first - _next;
            _next = first;
        }
        auto unread = events.Since(_next);
        _next = events.End();
        return unread;
    }
    // Forgets what is waiting: the next Read starts with the next event emitted.
    void Skip(const SimulationEvents<E>& events) { _next = events.End(); }
    // Events dropped before this reader saw them: it was not read for longer than one frame's worth of steps.
    [[nodiscard]] std::uint64_t Missed() const { return _missed; }

private:
    std::uint64_t _next = 0;
    std::uint64_t _missed = 0;
};
