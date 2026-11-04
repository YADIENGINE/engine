# Animation

Values over time; nothing here draws. Curves and named timelines serve gameplay and views. They have no clock and change nothing:
the caller owns the time and gets the same answer for the same position. So one set of tools works inside a
[simulation](Simulation.md), which counts steps, and in a view, which counts seconds. Design record:
`Docs/Design/README.md` → D28 (engine repository).

**Guarantee in a simulation:** the same as any [simulation](Simulation.md): the same build on the same machine.

## The parts

| Header | Use |
| --- | --- |
| `Animation/Easing.h` | Curves, and time helpers for one or two moments: `Progress01`, `Staggered`, `Crossed`, `CountDown` |
| `Animation/Timeline.h` | `StepTimeline`, `SecondsTimeline`: named moments on a line. `StepsIn` |
| `Simulation/TimelineMoments.h` | `AppendMoments`: a timeline's moments for `ctl seek --moment` |
| `Animation/SpringPoint.h`, `AnimatedCounter.h`, `ButtonAnim.h` | [Helpers for views](#helpers-for-views), on seconds |
| `Utilities/MathUtils.h`, `Utilities/Float2.h` | `Mix`: a value between two others |

Worked example: [AgentDocs/Recipes/Timeline.md](../AgentDocs/Recipes/Timeline.md).

## A value that animates

A tween is three pure pieces: how far along, which curve, between which values.

```cpp
const float t = Progress01(age, 0.0f, 0.4f);                 // How far along, 0..1
const float scale = Mix(0.5f, 1.0f, EaseOutBack(t));         // A curve shapes it; Mix makes it a value
const Float2 position = Mix(from, to, EaseInOutCubic(t));
```

No tween object exists, so there is nothing to start, update, forget or save. In a simulation, only the clock is state.

## Curves

Each takes 0..1 and returns exactly 0 at 0 and exactly 1 at 1; a value outside 0..1 is clamped. Some leave the range on the
way, on purpose.

| Family | Curves | Good for |
| --- | --- | --- |
| Linear | `EaseLinear` | Bars, constant motion |
| Quad, Cubic, Quart | `EaseIn…`, `EaseOut…`, `EaseInOut…` | Ordinary motion, from gentle to sharp |
| Sine | `EaseInSine`, `EaseOutSine`, `EaseInOutSine` | The softest: drifting, breathing |
| Expo | `EaseInExpo`, `EaseOutExpo` | A dash, a sudden stop |
| Back | `EaseInBack`, `EaseOutBack`, `EaseInOutBack` | A wind-up; a pop-in that overshoots |
| Elastic | `EaseInElastic`, `EaseOutElastic` | A spring hit |
| Bounce | `EaseInBounce`, `EaseOutBounce` | A landing |

For a value that follows a moving target instead of a fixed path, see `SpringPoint` below.

## Timelines

A timeline is constant data: moments with names, starting at 0 and never going back.

```cpp
StepTimeline attack{{"charge", 0}, {"fire", 45}, {"recover", 50}, {"cycle", 90}};
```

| Call | Answer |
| --- | --- |
| `ForEachCrossed(before, after, fn)` | Calls `fn(name)` for each moment with `before <= at < after`, in order |
| `Progress(from, to, position)` | How far between two moments, 0..1 |
| `Progress(from, to, step, alpha)` | The same for a view between steps (`StepTimeline` only) |
| `PhaseAt(position)` | The latest moment reached; empty before the first |
| `Wrap(position)` | An ever-growing clock mapped into one cycle |
| `PositionOf(name)`, `Contains(name)`, `Length()`, `Finished(position)`, `Moments()` | Plain facts |

Invalid data (no moments, empty or repeated names, negative positions, positions going back) and unknown names throw
`std::invalid_argument` in every configuration. Keep a timeline as a member, so the error appears when the simulation or scene
is created; a global would fail before `main`, with no message.

### Meeting moments

Advance the clock after asking, and every moment is met once and only once:

```cpp
void Boss::Step(const SimulationStep&) {
    attack.ForEachCrossed(_age, _age + 1, [&](std::string_view moment) {
        if (moment == "fire") Fire();
    });
    ++_age;
}
```

The range includes `before` and excludes `after`, so a moment at 0 is met by the first step, and the step whose clock is 45 meets
a moment at 45. A jump meets everything it passes, in order. Going back meets nothing. `Crossed` in `Easing.h` is older and
counts the other way (`before < moment <= after`), and is meant for a single moment on a view's clock; do not mix the two on one
clock.

Several moments at one position are met in the order declared.

### Repeating

```cpp
const int cycle = attack.Wrap(_age);
attack.ForEachCrossed(cycle, cycle + 1, onMoment);
```

The last moment marks the length and is then the same instant as position 0, so it is never met. Put what happens there at 0.

### Steps or seconds

- **`StepTimeline`, in a simulation.** Positions in a run are steps: whole numbers, compared exactly. Changing a simulation's step
  rate changes what a position means. Authors who think in seconds write `{"fire", StepsIn(1.5, 60)}`, which is rounded once, when the
  timeline is built.
- **`SecondsTimeline`, in a view or a menu.** On ordinary time, with no simulation.

The clock is a field of the game. In a simulation, it goes in `State`; the timeline does not, because it does not change. A timeline
built from `SimulationStart::parameters` is decided by the start, so it does not need to go in `State` either.

### Drawing between steps

A view reads the same timeline at the simulation's clock, and `alpha` from `SimulationRunner::Alpha`:

```cpp
const float charge = attack.Progress("charge", "fire", cycle, alpha);
paint.Circle(position, Mix(30.0f, 60.0f, EaseOutBack(charge)), Fill(color));
```

Like `StepSmoothed`, it runs from the previous step at alpha 0 to the current one at alpha 1. It assumes that `cycle` is the clock
the last step used. A `Wrap` makes the value jump back at the end of a cycle, on purpose.

A view does not call `ForEachCrossed` on a simulation's clock to change gameplay. To play a sound or start an effect, see
[effects are requests](Simulation.md#writing-a-simulation): a simulation asks through state the scene reads.

## Helpers for views

These hold state and advance by the seconds they are given, so they belong to a view or a menu. A simulation keeps to curves and
timelines: its deciding values must all go through `State`, and these keep some private.

| Type | Use |
| --- | --- |
| `SpringPoint` | A point that follows a moving target with a spring, such as a menu cursor; long frames are clamped |
| `AnimatedCounter` | A number on screen that runs up to its target and punches when it grows |
| `ButtonAnim` | A button's highlight, press and refusal as values; a theme's drawing reads them |

## Jumping to a moment

A simulation names moments for tools:

```cpp
std::vector<SimulationMoment> MyRules::Moments() const {
    std::vector<SimulationMoment> moments;
    AppendMoments(moments, _stage);                                       // Run steps, from step 0
    if (_bossStart >= 0) AppendMoments(moments, _attack, _bossStart, "attack.");
    return moments;
}
```

`python Engine ctl status` lists them under `moments`, and `python Engine ctl seek --moment boss` pauses there: that many steps have
run, and the next step is the one that meets the moment. So `ctl step --sim` shows what the moment does. A repeating timeline lists its
first cycle only.

These are steps the run knows now. A moment whose start depends on how the run goes appears once it is decided, so `attack.fire`
above does not exist before the boss comes in; a seek to it is refused, with the names that do exist. Two timelines with one name need
a prefix: names are keys, and the later one wins. The rules must not read `Moments`.

## Costs and limits

- Asking by name walks the moments and compares strings. Measured in Release: a wrap, a crossing check and an eased progress cost
  about 18 ns together on a timeline of 4 moments and 76 ns on 32, so 5,000 of them per step cost 0.09 and 0.38 ms.
- Sine, Expo and Elastic call the standard library's math; the others are plain arithmetic. All of them gave the same hashes in Debug
  and Release when measured, and other machines are outside the guarantee.
- These functions are in a header. A project that builds its game code with `/fp:fast` could get another copy of a curve inside its
  simulation, because the linker keeps one copy of each. Do not use `/fp:fast` in a project with a simulation. `StepSmoothed` is in
  the same position.
