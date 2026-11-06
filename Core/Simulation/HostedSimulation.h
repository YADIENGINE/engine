#pragma once
#include <utility>
#include "Simulation/SimulationHost.h"
#include "Simulation/SimulationRunner.h"

// A scene's simulation and runner: keep one as a member, built from SceneServices::simulation. A null host (the game)
// simply runs. In the editor a rewind or a loaded replay rebuilds the scene and replays the tape. T is constructed from
// a SimulationStart.
template <class T> class HostedSimulation {
public:
    HostedSimulation(SimulationHost* host, const SimulationSettings& settings, SimulationStart fallback)
        : _host(host)
        , _start(_host ? _host->StartFor(settings, std::move(fallback)) : std::move(fallback))
        , _simulation(_start)
        , _runner(_simulation, settings, _start) {
        if (_host) _host->Attach(_runner);
    }
    ~HostedSimulation() {
        if (_host) _host->Detach(_runner);
    }
    [[nodiscard]] T& Sim() { return _simulation; }
    [[nodiscard]] const T& Sim() const { return _simulation; }
    [[nodiscard]] SimulationRunner& Runner() { return _runner; }
    [[nodiscard]] const SimulationRunner& Runner() const { return _runner; }
    [[nodiscard]] const SimulationStart& Start() const { return _start; }

private:
    SimulationHost* _host;
    SimulationStart _start;
    T _simulation;
    SimulationRunner _runner;

public:
    HostedSimulation(const HostedSimulation&) = delete;
    HostedSimulation& operator=(const HostedSimulation&) = delete;
};
