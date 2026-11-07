#pragma once
#include <cstdint>
#include "Input/InputBindings.h"
#include "Scene/Scene.h"
#include "Simulation/ArenaSimulation.h"
#include "Simulation/SimulationEventReader.h"
#include "Simulation/SimulationRunner.h"
#include "Ui/Kernel/Ui.h"

// Hosts the arena's rules: feeds input, advances by game time and draws the state. Esc pauses; Quit asks the game
// to close (QuitRequested), since closing the game is the Game's job.
class PlayScene final : public Scene {
public:
    PlayScene(const SceneServices& services, std::uint64_t seed);
    [[nodiscard]] bool QuitRequested() const { return _quit; }

protected:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    // The menu is built on real time, so it moves while game time stands still.
    void UpdateReal(float seconds) override;
    void Update(float seconds) override;
    // Open menu: game time stops, so the rules, effects and hitstops wait.
    [[nodiscard]] bool GameTimePaused() const override { return _paused; }
    void Draw(Painter& paint) const override;

private:
    void PauseMenu();

    TimeScale& _time;
    const FontFace* _font;
    SimulationStart _start;
    ArenaSimulation _simulation;
    SimulationRunner _runner;
    InputBindings _bindings;
    SimulationEventReader<ArenaEvent> _events;
    Ui _ui;
    bool _paused = false;
    bool _quit = false;
};
