#include "Tests/Simulation/ToyProcedural.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include "Procedural/Caves.h"
#include "Simulation/StateVisitor.h"
#include "Utilities/Random/NoiseField.h"
#include "Utilities/Random/Scatter.h"

namespace {
constexpr Vec2 MapSize{48, 32};
constexpr NoiseSettings Drift{.frequency = 0.05f, .fractal = NoiseFractal::FBm, .seed = 11};
constexpr NoiseSettings Gust{.kind = NoiseKind::Perlin, .frequency = 0.2f, .seed = 12};
constexpr int LootEvery = 5;
} // namespace

const DrawTable& ToyProceduralLoot() {
    static const DrawTable table{
        .entries = {{.weight = 60.0}, {.weight = 30.0}, {.weight = 9.0}, {.weight = 1.0, .pity = 12}},
        .noRepeat = true};
    return table;
}

ToyProceduralSimulation::ToyProceduralSimulation(const SimulationStart& start) {
    _run.rng = Rng(start.seed);
    Rng layout(start.seed, 1); // The level has a stream of its own
    _run.map = GenerateCaves({.size = MapSize}, layout);
    for (const Float2 gem : ScatterPoints(FloatRect::From(_run.map.Bounds()), 3.5f, layout)) {
        const Vec2 cell{static_cast<int>(gem.x), static_cast<int>(gem.y)};
        if (_run.map[cell] == MapCell::Floor) _run.gems.push_back(gem);
    }
    _run.wisp = ToFloat2(MapSize) * 0.5f;
    _run.drops.assign(ToyProceduralLoot().entries.size(), 0);
}
void ToyProceduralSimulation::Step(const SimulationStep&) {
    const auto age = static_cast<float>(_run.age);
    const float dx = Noise(Drift, Float2{age, 0.0f});
    const float dy = Noise(Drift, Float2{0.0f, age});
    const Float2 pushed = Warp(Gust, 2.0f, _run.wisp);
    _run.noise += static_cast<double>(dx) + static_cast<double>(dy) + static_cast<double>(pushed.x + pushed.y);
    _run.wisp = {std::clamp(_run.wisp.x + dx * 0.5f, 0.0f, ToFloat2(MapSize).x),
                 std::clamp(_run.wisp.y + dy * 0.5f, 0.0f, ToFloat2(MapSize).y)};
    if (_run.age % LootEvery == 0) ++_run.drops[ToyProceduralLoot().Draw(_run.loot, _run.rng)];
    ++_run.age;
}
void ToyProceduralSimulation::State(StateVisitor& state) const { state.Fields(_run); }

SimulationDefinition ToyProceduralDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "engine.toy-procedural";
    definition.create = [](const SimulationStart& start) { return std::make_unique<ToyProceduralSimulation>(start); };
    return definition;
}
