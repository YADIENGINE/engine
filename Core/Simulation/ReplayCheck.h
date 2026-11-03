#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Modules/JsonIO/JsonIO.h"
#include "Simulation/InputTape.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/SimulationRunner.h"

// A fresh run of a tape against its stored hashes. Throws std::invalid_argument when the tape cannot play on the
// definition.
struct TapeVerification {
    std::uint64_t steps = 0;
    std::uint64_t hash = 0;               // Of the final state
    std::optional<TapeMismatch> mismatch; // Empty: every stored hash matched
    bool sameVersions = true;
};
[[nodiscard]] TapeVerification VerifyTape(const SimulationDefinition& definition, const InputTape& tape);

// Two fresh runs of a tape in this process, compared at every step. A difference is nondeterminism in the simulation,
// such as uninitialized memory, a clock or static state.
struct FieldDifference {
    std::string path;
    JsonIO::Document first;
    JsonIO::Document second;
};
struct RunDivergence {
    std::uint64_t step = 0; // The state after this many steps is the first that differs
    std::vector<FieldDifference> fields;
};
[[nodiscard]] std::optional<RunDivergence> FindDivergence(const SimulationDefinition& definition,
                                                          const InputTape& tape);

// The state after 'step' steps of a fresh run; at most the tape's end.
struct TapeState {
    std::uint64_t step = 0;
    std::uint64_t hash = 0;
    JsonIO::Document state;
};
[[nodiscard]] TapeState StateAt(const SimulationDefinition& definition, const InputTape& tape, std::uint64_t step);
