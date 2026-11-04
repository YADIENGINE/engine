#pragma once
#include <cstdint>
#include <string_view>

class StateVisitor;

// A simulation's events as tools see them, whatever their type: `sim run`, scenario checks and the editor. Events are
// numbered in the order they were emitted, from 0; only those of the last few steps are kept (SimulationEvents).
// Implemented by SimulationEvents; a simulation returns its own from Simulation::EventLog.
class SimulationEventLog {
public:
    virtual ~SimulationEventLog();

    // The number the next event will get. Kept events are numbered [FirstKept(), End()).
    [[nodiscard]] virtual std::uint64_t End() const = 0;
    [[nodiscard]] virtual std::uint64_t FirstKept() const = 0;
    // A kept event's step, name and fields (the event's own State); 'sequence' must be in [FirstKept(), End()).
    [[nodiscard]] virtual std::uint64_t StepOf(std::uint64_t sequence) const = 0;
    [[nodiscard]] virtual std::string_view NameOf(std::uint64_t sequence) const = 0;
    virtual void FieldsOf(std::uint64_t sequence, StateVisitor& fields) const = 0;

protected:
    SimulationEventLog() = default;
    SimulationEventLog(const SimulationEventLog&) = default;
    SimulationEventLog& operator=(const SimulationEventLog&) = default;
};
