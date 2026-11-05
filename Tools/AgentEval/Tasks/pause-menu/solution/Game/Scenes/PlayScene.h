#pragma once
#include <cstdint>
#include "Input/InputBindings.h"
#include "Runtime/Game.h"
#include "Scene/Scene.h"
#include "Simulation/ArenaSimulation.h"
#include "Simulation/SimulationEventReader.h"
#include "Simulation/SimulationRunner.h"
#include "Ui/Kernel/Ui.h"

// Hosts the arena's rules: feeds input, advances by game time and draws the state. Esc pauses.
class PlayScene final : public Scene {
public:
    PlayScene(Game& game, std::uint64_t seed);
    ~PlayScene() override;

protected:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;

private:
    void PauseMenu();

    Game& _game;
    SimulationStart _start;
    ArenaSimulation _simulation;
    SimulationRunner _runner;
    InputBindings _bindings;
    SimulationEventReader<ArenaEvent> _events;
    Ui _ui;
    bool _paused = false;
};
