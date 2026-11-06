#pragma once
#include <vector>
#include "Procedural/MapCell.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/Grid.h"
#include "Utilities/Random/DrawTable.h"
#include "Utilities/Random/Rng.h"

// A level generated from the seed, used only to test that procedural tools replay exactly: caves, gems scattered over
// their floor, a wisp that drifts on smooth noise, and loot drawn from a table with pity. Every result feeds the hash,
// so noise or scattering that differed between runs would show.
struct ToyProceduralRun {
    Rng rng;
    Grid<MapCell> map;
    std::vector<Float2> gems; // In cells, on floor
    Float2 wisp;
    DrawMemory loot;
    std::vector<int> drops; // How often each loot entry came up
    int age = 0;
    double noise = 0.0; // Every noise value read so far, summed
};

class ToyProceduralSimulation final : public Simulation {
public:
    explicit ToyProceduralSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] const ToyProceduralRun& Run() const { return _run; }

private:
    ToyProceduralRun _run;
};

[[nodiscard]] SimulationDefinition ToyProceduralDefinition();
// The loot table the toy draws from; its last entry is rare and has pity.
[[nodiscard]] const DrawTable& ToyProceduralLoot();
