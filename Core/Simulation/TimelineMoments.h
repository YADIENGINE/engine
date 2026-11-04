#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Animation/Timeline.h"
#include "Simulation/Simulation.h"

// A timeline's moments for Simulation::Moments. 'startStep' is the run step at which the timeline's clock was 0;
// 'prefix' keeps names of several timelines apart ("boss." gives "boss.fire").
inline void AppendMoments(std::vector<SimulationMoment>& moments, const StepTimeline& timeline,
                          std::uint64_t startStep = 0, const std::string& prefix = {}) {
    for (const auto& moment : timeline.Moments())
        moments.push_back({prefix + moment.name, startStep + static_cast<std::uint64_t>(moment.at)});
}
