# Simulation and replay

Gameplay can run as a deterministic simulation: fixed steps, a seed and per-step input decide everything. A run can then
be replayed, checked by hash, and driven without a window by a test or an agent. Design record: `Docs/Design/README.md` →
D26 (engine repository).

**Guarantee:** the same build on the same machine. Debug and Release gave identical hashes when measured, but that is
not promised across compilers or CPUs, and a replay is not a save format across game versions. There is no online play.

Use a simulation for a game's rules. Menus, transitions and effects stay ordinary scenes and views on variable time.

## The parts

| Header | Use |
| --- | --- |
| `Simulation/Simulation.h` | Your rules: `Step` and `State` |
| `Simulation/SimulationSettings.h` | `SimulationSettings`: id, step rate, players, action names. `SimulationStart`: seed, players, parameters |
| `Simulation/SimulationRunner.h` | Bridges frames and steps; records and replays |
| `Simulation/StepSmoothed.h` | A value drawn between steps |
| `Simulation/SimulationEvents.h`, `Simulation/SimulationEventReader.h` | What happened in a step, for the view: [Events](#events) |
| `Simulation/SimulationRegistry.h` | `RegisterSimulations`: what tools and tests can run |
| `Simulation/InputTape.h` | A recorded run, as a file |
| `Simulation/ReplayCheck.h` | `VerifyTape`, `FindDivergence`, `StateAt` |
| `Simulation/Scenario.h` | Input, steps and state from a C++ test |
| `Input/InputBindings.h` | Keys to per-player actions |
| `Utilities/Random/Rng.h`, `Utilities/Random/WeightedPick.h` | Seeded random numbers, weighted choice |
| `Utilities/Random/`, `Procedural/` | Loot with pity, scattered points, noise, levels generated from the seed: [Procedural](Procedural.md) |
| `Utilities/Pool.h` | `Pool<T>` and `Handle<T>`: things that come and go, with an identity that lasts: [below](#things-that-come-and-go) |
| `Physics/Collision/CollisionWorld.h` | Hits and overlaps inside the rules: [Collision](Collision.md) |
| `Animation/Animator.h`, `Animation/Tween.h`, `Animation/Timeline.h` | Character clips, moving values, boss and wave timing: [Animation](Animation.md) |
| `Camera/Camera2D.h` | A camera that decides gameplay (enemies wake as it shows them, no scrolling back): [Camera](Camera.md#a-camera-in-the-rules) |
| `Simulation/HostedSimulation.h` | A scene's simulation and runner; the editor keeps its tape |

Worked example: [AgentDocs/Recipes/Simulation.md](../AgentDocs/Recipes/Simulation.md).

## Writing a simulation

A simulation is plain state. It has no window, drawing, audio, clock or global random generator; it lives in
`Game/Simulation/` and links only low-level engine targets, so reaching a renderer fails to build, and `python Engine
check` names forbidden includes. It may read [tuning](Tuning.md) values.

`Step` receives one fixed step: its index, its duration, each player's input and any commands. `State` visits every field
that decides later steps, including each `Rng`. That one function feeds the hash, the readable dump and scenario checks.

### State

Keep what decides the game in one plain struct and visit it whole, so a field added later cannot be left out of the hash:

```cpp
struct RunState {
    Rng rng;
    StepSmoothed<float> shipX;   // Its current value
    StepTween<float> recoil{0};  // Engine types with a State of their own write it as a group
    std::vector<Enemy> enemies;  // enemies[0].hp, ...; Enemy is a plain struct too
    int score = 0;
};
void MySimulation::State(StateVisitor& state) const { state.Fields(_run); }
```

`Fields` visits every field by its own name in declaration order; `Field("run", _run)` does the same inside a `run.` group.
`Field` also takes lists (`std::vector`, `std::array`), `std::optional` and anything with a `State(visitor)` member, so
`state.Field("camera", _camera)` is one line. A plain struct has public fields only, no constructor or base class, and is
declared outside any function and anonymous namespace ([`Utilities/Reflection.h`](../Core/Utilities/Reflection.h)). What
only the view uses (drawing caches, sound handles) stays outside the struct. Moving a field changes the hash; tapes belong
to one build anyway. Writing fields one by one (`state.Field("score", _score)`) still works, for values computed from
others or a class with private members.

Rules that keep a run reproducible:

- **Everything comes from the start or the steps.** Options, saved progress and menu choices arrive through
  `SimulationStart` parameters or a command.
- **Own your generators.** Build an `Rng` from `start.seed`, with a stream number per purpose if you like, so a new roll
  in one system does not move another. Visual code keeps its own generator and cannot touch these.
- **No hidden order.** Do not iterate an `unordered_map` keyed by pointers, read static variables or leave members
  uninitialized. `sim diverge` finds these.
- **No threads, no clock.**
- **State goes field by field.** Never hash raw struct bytes: measured, padding differs between two runs of one program.
  `Fields` walks a struct's fields, which is not its bytes.
- **Effects are requests.** To get hitstop or a sound, emit an [event](#events) the scene reads; the simulation does not
  call `TimeScale`, `Audio` or `Fx`.

Simulation code is always built with `/fp:precise`. Measured: `/fp:fast` changes results between Debug and Release,
because Release switches to vector math.

### Things that come and go

Enemies, bullets and pickups that spawn and die go in a `Pool<T>` (`Utilities/Pool.h`) rather than a `std::vector`, as
soon as anything points at them. `Add` gives a `Handle<T>`; a handle finds its item until that item is removed, whatever
else is added or removed, and finds nothing afterwards. Keep handles in the state for anything that refers to another
thing (a target, a parent, a leader); an index into a list names the wrong item once an earlier one is removed.

```cpp
struct Missile { Float2 at; Handle<Drone> target; };
struct RunState {
    Pool<Drone> drones;
    Pool<Missile> missiles;
};

for (Missile& missile : _run.missiles)
    if (Drone* drone = _run.drones.Find(missile.target)) ... // Null once the drone is gone
_run.drones.RemoveIf([](const Drone& drone) { return drone.hp <= 0; });
```

- **Order:** items stay in spawn order. A removed item leaves, the others keep their order, new items go to the end.
- **Handles:** typed (a `Handle<Drone>` is no `Handle<Missile>`), plain values, `{}` is null. `Get` throws for a handle
  that names nothing; `Find` gives null. A pointer from `Find` or `Get` is for now: keep the handle.
- **No adding or removing inside a loop over the same pool:** it throws `std::logic_error`. Spawn before or after the
  loop; remove with one `RemoveIf` per step.
- **State:** a pool writes its items with their handles (`drones[0].handle.index`, `drones[0].hp`) and its free slots,
  because those decide the handles given later. The same operations give the same handles.
- **Colliders:** a `PoolColliders` value declared beside the item type adds, moves and removes the items' colliders
  with them and leads a hit back to its item: [Collision](Collision.md#items-in-a-pool).

Worked example: [AgentDocs/Recipes/Pool.md](../AgentDocs/Recipes/Pool.md).

## Step rate

`stepsPerSecond` is a whole number from 1 to 240, default 60. It is fixed for all runs of a simulation. Fast bullets are a
reason for a swept test, not for a higher rate.

## Hosting it in a scene

The runner sits between variable frames and fixed steps. A scene in `TimeDomain::Game` keeps it in a `HostedSimulation<T>`
built from `SceneServices::simulation` ([In the editor](#in-the-editor)), feeds it in `HandleInput`, advances it in
`Update`, cancels it in `InputCancelled`, and draws with `Alpha`:

| Situation | What happens |
| --- | --- |
| A frame owes no step | A press waits for the next step |
| A frame owes several steps | A press reaches the first one only; held state reaches all |
| A tap shorter than a step | `Pressed` is true, `Held` false |
| Hitstop or slow motion | `TimeScale` scales the seconds, so only steps per real second change; nothing is recorded |
| Scene paused | It neither feeds nor advances |
| Focus lost, scene left | `CancelInput` drops presses and releases held actions |
| A long hitch | At most a quarter of a second of steps runs; the rest is dropped |

Playback seconds are therefore not proportional to steps. Steps are the exact position in a run.

Values that move each step use `StepSmoothed<T>`: `Set` in the step, `At(runner.Alpha())` when drawing. Drawing lags the
state by up to one step, which is what makes it smooth. `Snap` teleports without drawing a line across the jump.

## Events

A simulation tells its view what happened (a hit, a death, a pickup) by emitting events. The view plays sounds, effects
and hitstop for them. An event is a plain value that names itself; tools show its fields by their own names:

```cpp
struct Hit {
    static constexpr std::string_view Name = "Hit"; // How tools name it
    Float2 at;
    int enemy = 0;
};
using GameEvent = std::variant<Hit, Death, Pickup>; // One list keeps their order
```

The simulation keeps a `SimulationEvents<GameEvent>` member, calls `Emit(step, Hit{...})` during `Step`, exposes it to
the view and returns it from `EventLog()` for tools. The scene keeps a `SimulationEventReader<GameEvent>` and reads after
advancing:

```cpp
_runner.Advance(seconds);
for (const auto& happened : _reader.Read(_simulation.Events(), _runner))
    std::visit(Overloaded{[&](const Hit& hit) { _fx.Play("hit", {.at = hit.at}); },
                          [&](const Death&) { _sounds.Play(Sfx::Death); }, [](const Pickup&) {}},
               happened.event);
```

| Question | Answer |
| --- | --- |
| A frame ran several steps | Every event of every step arrives, in order, each with its `step` and `sequence` |
| Several things react | Each keeps its own reader; reading does not consume for the others |
| State and hash | Events are output: leave them out of `State`. A replay emits the same ones because it runs the same steps |
| How long they last | The steps of one frame's longest catch-up (`SimulationMaxFrameSeconds`) and one more; read every frame. `Missed()` counts what a reader lost |
| Seeking, `RunTo` | Catching up is silent: the runner's `EventsSilencedUntil` makes `Read` skip those steps, so a seek does not play 600 sounds. `ReadAll` includes them, for what must reach a final value; a total belongs in the state anyway |
| Feeding back into the rules | Never: an event the rules need next step is state. Keep it in a member and in `State` |

`Utilities/Overloaded.h` builds the visitor; a missing alternative does not compile, so a new event type shows every place
that must handle it. Events are named in [scenarios](#scenarios) and by `sim run --events`.

## Input and tapes

A tape stores actions, not keys. The game declares button and axis names, and `InputBindings` turns keys into per-player
actions before the runner sees them, so a tape does not depend on key layout or devices. It also means a replay does not
reproduce a binding mistake: test bindings on their own. A new device changes bindings, not the format. An [input
map](InputMap.md) names the buttons and axes once and gives each player's default keys: `DeclareInput<XInput>` fills
the declaration and `InputBindings::BindMap<XInput>` binds the keys.

Axes are rounded to 1/1024 before the simulation sees them, so live play and replay read one value. `Command(id, value)`
carries events that are not keys, such as picking an upgrade.

A tape is JSON with sparse changes, so it can be read and written by hand:

```json
{
  "format": 1, "simulation": "my.play", "simulationVersion": 1, "engine": "0.6.0", "configuration": "Release",
  "stepsPerSecond": 60, "players": 2, "buttons": ["Left", "Right", "Fire"], "axes": ["AimX"],
  "seed": 42, "start": { "difficulty": 2 }, "steps": 400,
  "input": [
    { "at": 120, "player": 0, "held": ["Right"], "pressed": ["Right"] },
    { "at": 134, "player": 1, "pressed": ["Fire"], "axes": { "AimX": 512 } }
  ],
  "commands": [ { "at": 300, "command": "pick_upgrade", "value": 3 } ],
  "hashEvery": 60, "hashes": "8d2f...16 hex digits per hash"
}
```

A change holds a player's whole input from that step on, except `pressed`, which applies to that step only.

**Hashes** are FNV-1a 64 over the state, at step 0 and every `hashEvery` steps: one second of steps by default,
`SetHashEvery(1)` for a precise record. Editor and `sim run` tapes hash every step.

**Playing a different tape:** another simulation id, step rate or action list refuses to play. A different engine or
simulation version is reported and the hashes decide. Configuration is information only. Raise
`SimulationSettings::version` when a change makes old tapes play differently. Tuning belongs to the build, not to the
tape: replaying under changed values gives a different run, and the check says so. A tape recorded in a development build
also stores a digest of each tuning file's values (`"tuning": {"Game/Glow.tuning.json": "<hash>"}`), so `sim verify`
names the files whose values changed since in `tuningChanged` ([Tuning](Tuning.md#tapes)).

A player watches a replay with `Play(tape, AfterTape::Stop)`. With `AfterTape::Live`, input takes over when the tape ends,
and a press or a command during it cuts the tape there.

## Tools

`python Engine sim` builds a console tool from `Game/Simulation` and runs it without a window. Each answer is one JSON
object on stdout; build output goes to stderr. All take `--release`.

| Command | Answer | Exit 1 |
| --- | --- | --- |
| `sim list` | `simulations`: id, version, step rate, players, buttons, axes | |
| `sim run SCENARIO [--out TAPE] [--events]` | `passed`, `steps`, `hash`, `failures`, `reads`, `events` (a count per name); with `--events`, `eventList`: each event's `step`, `name` and `fields` | An expectation failed |
| `sim verify TAPE` | `match`, `hashes`, `steps`, `hash`, `hashEvery`, `sameVersions`, `recorded`; `mismatch` with `lastMatch`, `firstMismatch`; `reason` when the tape holds no hashes; `tuningChanged` when tuning values differ from the recording | The hashes differ, or there are none to compare |
| `sim diverge TAPE` | `deterministic`; `step` and `fields` (`field`, `first`, `second`) when two runs differ | Two runs differ |
| `sim state TAPE [--step N]` | `step`, `hash`, `state` (the flat dump) | |

Exit 2 is a bad request: wrong arguments, a tape that cannot play, an unreadable file, or no simulation in the project.

Finding a difference: `sim verify` compares a tape with today's build and reports where the first difference lies. With
`hashEvery` 1 that is one exact step; otherwise it is a range. `sim diverge` runs a tape twice in one process and names the
first differing step and fields, which catches nondeterminism. `sim state --step N` reads any moment. To compare Debug and
Release, record with one and `sim verify` with the other.

## Scenarios

A scenario gives input, runs steps and checks state, without a window:

```json
{
  "simulation": "my.play", "seed": 42, "players": 2, "start": { "difficulty": 2 },
  "script": [
    { "hold": "Right" },
    { "run": 30 },
    { "release": "Right" },
    { "tap": "Fire", "player": 1 },
    { "run": 1 },
    { "command": "pick_upgrade", "value": 3 },
    { "until": { "wave": 2 }, "limit": 600 },
    { "expect": { "score": 10, "player[0].lives": { "min": 1 } } },
    { "untilEvent": "Hit", "fields": { "enemy": 2 }, "limit": 300 },
    { "run": 60 },
    { "expectEvent": "Death", "count": 0 },
    { "read": ["score", "player[0].x"] }
  ]
}
```

| Operation | Meaning |
| --- | --- |
| `hold`, `release` | A button stays down from the next step, or comes up. `player` defaults to 0 |
| `tap` | Pressed and held for the next step only |
| `axis` + `value` | An axis, from the next step |
| `command`, `value` | A command for the next step |
| `run` | Run that many steps |
| `until`, `limit` | Run until the fields match; failing after `limit` steps (default 3600) |
| `expect` | Check fields now. A number, a string, a bool, or `{ "min": a, "max": b }`; floats compare within a small tolerance |
| `read` | Add these fields to `reads`, with the step and hash |
| `expectEvent`, `fields`, `count` | Count the [events](#events) of that name, and with those fields, emitted since the last event check. `count` is a number or `{ "min", "max" }`; default at least one |
| `untilEvent`, `fields`, `limit` | Run until such an event is emitted after the last event check; failing after `limit` steps (default 3600) |

Field names are the ones `State` writes: `score`, `player[0].lives`, `boss.position.x`. An unknown field or action is an
error (exit 2), not a failed check. C++ tests use `Scenario`, the same driver, with the simulation's own getters.

## In the editor

The scene the game hosts is the one the editor opens. It keeps a `HostedSimulation<T>` built from
`services.simulation`: null in the game, where the rules simply run; in the editor the worker records its tape across
resets. The toolbar shows the step and whether a replay is playing, and Step advances one simulation step. A simulation can
name points of its run with `Moments`, for `ctl seek --moment` ([Animation](Animation.md#jumping-to-a-moment)).

In a scene with a simulation the tape is the only record. Viewport input and `ctl action` both reach it: an action that
changes gameplay calls `Runner().Command(...)`. An action that only changes the view should be a parameter, because
parameters survive a reset; other actions are lost on a rewind. Rewinding rebuilds the scene and replays the tape from
step 0 with current parameters, keeping what follows, so a value can be changed and watched under the same input. Playing
on replays the rest of the tape and then goes live. A press or a command during that replay cuts the tape there; mouse
movement alone does not.

`ctl` works in exact steps with `step --sim N`, `seek --step N` and `frames --steps`, and `ctl replay load|save FILE` opens
and writes tapes ([Editor automation](Editor/Automation.md)). A loaded replay keeps its own start and hashes, and `ctl
status` reports a `mismatch`. A Build & Refresh starts a new worker, so the tape does not carry over.
