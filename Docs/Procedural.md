# Procedural generation

Levels, loot and scattered things made from a seed. Randomness lives in `Utilities/Random`; generated layouts live in
`Procedural/`. Everything here is plain functions over plain data, built with precise floats and linked into every
simulation, so a simulation may generate its level at the start or roll loot on any step and still replay exactly.
Design record: `Docs/Design/README.md` → D49 (engine repository).

**Guarantee:** the same as a [simulation](Simulation.md): the same build on the same machine gives the same result for
the same seed. The layout generators use whole numbers almost everywhere, but that is not promised across compilers.

## The parts

| Header | Use |
| --- | --- |
| `Utilities/Random/Rng.h` | `Rng`: the seeded generator everything here takes |
| `Utilities/Random/WeightedPick.h` | `WeightedIndex(weights, rng)`: a choice in proportion to weights, with no memory |
| `Utilities/Random/DrawTable.h` | `DrawTable` + `DrawMemory`: weighted draws that remember, with no-repeat and pity |
| `Utilities/Random/Scatter.h` | `ScatterPoints`: points spread evenly over an area (Poisson disk) |
| `Utilities/Random/NoiseField.h` | `Noise`, `Warp`: smooth noise over a plane or along a line ([Rendering](Graphics/Rendering.md#geometry-and-timing)) |
| `Utilities/Random/Noise.h` | `Hash`, `Random01`, `NoiseHash`: stable numbers for decorations |
| `Utilities/Geometry/Grid.h` | `Grid<T>`: a rectangle of cells addressed by `Vec2`; a simulation keeps one in its state |
| `Procedural/MapCell.h` | `MapCell::Wall` / `Floor`: what a generated layout says about a cell |
| `Procedural/Rooms.h` | `GenerateRooms`: rooms joined by corridors |
| `Procedural/RandomWalk.h` | `GenerateRandomWalk`: winding tunnels dug by a walker |
| `Procedural/Caves.h` | `GenerateCaves`: rounded caverns from a cellular automaton |
| `Procedural/Regions.h` | `FloorRegions`, `FillSmallRegions`, `ConnectRegions`: tidy a map of your own |

Worked example: [AgentDocs/Recipes/Procedural.md](../AgentDocs/Recipes/Procedural.md).

## A level from the seed

Generate in the simulation's constructor from `start.seed`, with an `Rng` stream of its own, and keep the result in the
state:

```cpp
MySimulation::MySimulation(const SimulationStart& start) {
    _run.rng = Rng(start.seed);
    Rng layout(start.seed, 1); // A stream of its own: generating does not move the rolls of play
    _run.map = GenerateCaves({.size = {64, 36}}, layout); // Grid<MapCell> in the state struct
}
```

Every generator keeps the map's border wall and returns one connected floor area, so every floor cell can reach every
other. What a cell becomes (tiles, colliders, decoration) is the game's choice. Keep the map in the state even if it
never changes: the hash then covers it, and a generator change shows as a replay difference instead of a silent one.

| Generator | Looks like | Settings worth changing |
| --- | --- | --- |
| `GenerateRooms` | Rectangular rooms, L-shaped corridors, no loops | `minRoom`, `maxRoom`, `corridorWidth` |
| `GenerateRandomWalk` | Organic tunnels and chambers | `floorShare`, `start` |
| `GenerateCaves` | Rounded caverns | `wallChance` (0.4 open … 0.55 narrow), `smoothing`, `minRegion` |

`RoomLayout` also lists the rooms (`Rect` in cells) and which rooms each corridor joins, for placing a start, an exit or
a boss room.

## Scattering

`ScatterPoints(area, spacing, rng)` gives points no closer than `spacing` that leave no gap where another would fit: trees,
stars, pickups, spawn spots. To place on a map, scatter over its bounds in cells and keep the points that land on floor.
Points come in the order they were placed, which is a function of the generator alone.

## Tables that remember

`WeightedIndex` forgets: a 1% drop can stay away for hundreds of rolls. A `DrawTable` can promise it:

```cpp
const DrawTable loot{.entries = {{.weight = 70}, {.weight = 29}, {.weight = 1, .pity = 30}}, .noRepeat = true};
struct Run { Rng rng; DrawMemory lootMemory; }; // In the state
const std::size_t drop = loot.Draw(_run.lootMemory, _run.rng);
```

- `pity = N`: after N draws in a row without it, the entry comes up for sure. When several are due, the most overdue
  wins. An entry with weight 0 and a pity comes up only by pity.
- `noRepeat`: the last entry never comes up twice in a row while another could.
- The table is fixed data; `DrawMemory` is all it remembers. Keep that in the state and replays keep their pity.

## Noise in the rules

Noise is a plain function of its settings and point. A simulation may read it every step (a drifting hazard, wind, a
wandering patrol); the value depends only on what goes in, so there is nothing to save. Feed it step-based inputs
(`age`, cell coordinates), never seconds from a clock. Drawing may use the same noise for shading, which decides nothing.
