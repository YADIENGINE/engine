#include "Simulation/SimulationSettings.h"
#include <stdexcept>
#include "Simulation/PlayerInput.h"

namespace {
constexpr int MaxStepsPerSecond = 240;
}

void ValidateSimulation(const SimulationSettings& settings) {
    if (settings.id.empty()) throw std::invalid_argument("A simulation needs an id");
    if (settings.stepsPerSecond < 1 || settings.stepsPerSecond > MaxStepsPerSecond)
        throw std::invalid_argument(settings.id + ": stepsPerSecond must be between 1 and 240");
    if (settings.maxPlayers < 1) throw std::invalid_argument(settings.id + ": maxPlayers must be at least 1");
    if (settings.buttons.size() > MaxPlayerButtons)
        throw std::invalid_argument(settings.id + ": at most 32 buttons per player");
    if (settings.axes.size() > MaxPlayerAxes) throw std::invalid_argument(settings.id + ": at most 8 axes per player");
}
void ValidateSimulationStart(const SimulationSettings& settings, const SimulationStart& start) {
    if (start.players < 1 || start.players > settings.maxPlayers)
        throw std::invalid_argument(settings.id + ": players must be between 1 and maxPlayers");
}
