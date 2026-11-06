#pragma once
#include <cstdint>
#include "Input/InputBindings.h"
#include "Runtime/TimeScale.h"
#include "Scene/Scene.h"
#include "Simulation/ArenaSimulation.h"
#include "Simulation/SimulationEventReader.h"
#include "Simulation/SimulationRunner.h"

// Hosts the arena's rules: feeds input, advances by game time and draws the state.
class PlayScene final : public Scene {
public:
    PlayScene(TimeScale& time, std::uint64_t seed);

protected:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;

private:
    TimeScale& _time;
    SimulationStart _start;
    ArenaSimulation _simulation;
    SimulationRunner _runner;
    InputBindings _bindings;
    SimulationEventReader<ArenaEvent> _events;
};
