---
name: simulation
description: Test or reproduce gameplay without a window, or write gameplay rules as a simulation. Use to play a scenario with input and check state, reproduce a bug from a replay, find where a run stopped matching, or check determinism. Driven with `python Engine sim`.
---

# Play gameplay without a window

Commands run from the project root. Reference: `Docs/Simulation.md`. Example: `AgentDocs/Recipes/Simulation.md`.

1. **Find it:** `python Engine sim list` gives the simulations with their button and axis names. Exit 2 with "No simulation
   sources" means the rules are not a simulation yet: see the recipe before writing one.
2. **Read the state's names:** they are what the simulation's `State` writes. `python Engine sim state TAPE` lists them
   when you have a tape; otherwise read `State` in `Game/Simulation`.
3. **Write a scenario** (a JSON file under `Game/Simulation/Scenarios/`, for instance) that holds, taps and runs, then
   checks with `expect`. Prefer `until` with a `limit` to guessing step counts.
4. **Run it:** `python Engine sim run FILE --out run.replay`. Exit 1 lists `failures` with step, field, expected and actual.
   Add `read` operations to see values along the way.
5. **Fix and repeat.** Report the scenario that now passes and what it checks. A passing scenario says nothing about how
   the game looks: for that use `AgentDocs/Skills/visual-change/SKILL.md`.

## A replay that no longer matches

- `python Engine sim verify run.replay`: `mismatch` says where. With `hashEvery` above 1 it is a range.
- `python Engine sim diverge run.replay`: two runs of the same tape. `deterministic: false` names the step and fields, so
  the simulation has a hidden input: static state, uninitialized memory, a clock, unordered iteration.
- `python Engine sim state run.replay --step N` reads any moment. A tape is plain JSON: cut its `input` to isolate a bug.

Changed rules or tuning also make a tape differ, and then the mismatch is expected. Say which you think it is.

## Rules when writing simulation code

Everything that decides gameplay comes from `SimulationStart` or the steps. No drawing, audio, clocks or `<random>`; own an
`Rng` from `start.seed`; name every deciding field in `State`, the `Rng` included. `python Engine check` reports forbidden
includes. Add new state to `State` in the same change. Collision inside the rules: `AgentDocs/Skills/collision/SKILL.md`. Timing inside the rules:
`AgentDocs/Skills/timeline/SKILL.md`.

## In the editor

Open scenes with a simulation work in exact steps: `python Engine ctl seek --step N` or `--moment NAME`, `ctl step --sim N`, `ctl frames
--steps --from A --to B`. `ctl replay load FILE` plays a tape in the open scene, so a failing scenario can be watched.
Prefer steps to seconds there: hitstop makes seconds and steps drift apart.
