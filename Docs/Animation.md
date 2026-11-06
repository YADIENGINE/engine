# Animation

Values over time, for gameplay and for views. Nothing here draws: it works out a number, a pose or a moment, and the game or the
view uses it. Everything comes in two forms: on whole steps inside a [simulation](Simulation.md), and on seconds in a view or a
menu. Design record: `Docs/Design/README.md` → D28 (engine repository).

**Guarantee in a simulation:** the same as any [simulation](Simulation.md): the same build on the same machine.

## Which one

| You want | Use | Recipe |
| --- | --- | --- |
| A value to go somewhere: a pop-in, a cursor, a recoil | [`Tween`](#tweens) | [Tween](../AgentDocs/Recipes/Tween.md) |
| Such values without a field for each | [`TweenSet`](#tween-sets) | [Tween](../AgentDocs/Recipes/Tween.md) |
| One value through several fixed points | [`Track`](#tracks) | - |
| A character with idle, walk, attack, and fades between them | [`Animator`](#animators) with [clips](#clips-and-poses) | [Animator](../AgentDocs/Recipes/Animator.md) |
| A boss's plan or a wave schedule: named moments that decide things | [`Timeline`](#timelines) | [Timeline](../AgentDocs/Recipes/Timeline.md) |
| Just a formula | [Curves](#curves) | - |

| Header | Use |
| --- | --- |
| `Animation/Tween.h`, `TweenSet.h` | `StepTween`, `SecondsTween`, `UpdateAll`; `StepTweenSet`, `SecondsTweenSet` |
| `Animation/Track.h` | `StepTrack`, `SecondsTrack`: a value through keys |
| `Animation/Pose.h` | `AnimationPose`, `MixPose`: what a pose is |
| `Animation/Clip.h`, `ClipSet.h` | `StepClip`, `SecondsClip`; `StepClipSet`, `SecondsClipSet` |
| `Animation/Animator.h` | `StepAnimator`, `SecondsAnimator` |
| `Animation/Timeline.h` | `StepTimeline`, `SecondsTimeline`: named moments. `StepsIn` |
| `Animation/Curve.h`, `Easing.h` | `Curve` and `Ease`; the same curves as functions, and small time helpers |
| `Simulation/TimelineMoments.h` | `AppendMoments`: a timeline's moments for `ctl seek --moment` |
| `Animation/SpringPoint.h`, `AnimatedCounter.h`, `ButtonAnim.h` | [Helpers for views](#helpers-for-views) |
| `Animation/Flicker.h` | `Flicker`: a lamp's strength from the time, [with no state](Graphics/Lighting.md#flicker) |

Any type can be animated if it has a `Mix(a, b, t)` beside it, as `float`, `Float2` and `Color4` do. Whole numbers, flags and enums
have no "between": they hold, then change.

## Steps or seconds

Each part has both forms, and they differ only in the name and in how they advance:

| | In a simulation | In a view or a menu |
| --- | --- | --- |
| Names | `StepTween`, `StepClip`, `StepAnimator`, ... | `SecondsTween`, `SecondsClip`, `SecondsAnimator`, ... |
| Positions | Whole steps, compared exactly | Seconds |
| Advance | `Step()`, once per `Simulation::Step` | `Update(deltaSeconds)`, once per frame |
| Draw | `Value(alpha)`, `Pose(alpha)`, with `alpha` from `SimulationRunner::Alpha` | `Value()`, `Pose()` |
| Save | `State(state)`, inside `Simulation::State` | - |

A `1.5` in a step position does not compile. Authors who think in seconds write `{StepsIn(1.5, 60), value}`, which is rounded once
when the data is built; changing a simulation's step rate changes what a position means.

Drawing with `alpha` runs from the previous step at alpha 0 to the current one at alpha 1, like `StepSmoothed`.

## Tweens

A tween is the value itself, told where to go. Keep it as a field, where the plain value would have been:

```cpp
struct Enemy {
    SecondsTween<float> scale{1.0f};
    Float2 at;
};

enemy.scale.To(1.3f, 0.1f, Curve::OutQuad).Then(1.0f, 0.2f);    // When hit
UpdateAll(deltaSeconds, enemies, &Enemy::scale);                // Each frame, the whole list
paint.Circle(enemy.at, 20.0f * enemy.scale, Fill(color));       // It reads like the float
if (enemy.scale.JustFinished()) PlaySound();                    // In the update it ended
```

| Call | Does |
| --- | --- |
| `To(target, duration, curve)` | Replaces what was running and goes from the current value, so it never jumps |
| `Then(target, duration, curve)` | Goes on from the end of the chain |
| `Wait(duration)` | Holds for a while, in the chain |
| `Loop()` | Repeats the chain, each round from where the last ended. It never finishes |
| `Stop()`, `Snap(value)` | Stays where it is; jumps with no movement. Both forget the chain |
| `Update(amount)`, `Step()` | Advances. True in the call the chain ended |
| `Value()` or the plain value | What it is now. Use `Value()` where C++ cannot work out the conversion |
| `Active()`, `JustFinished()` | A chain is running; it ended in the last update |

With no curve a part is `Curve::Linear`.

**One `Update` line.** `UpdateAll(deltaSeconds, a, b, c)` covers several, and `UpdateAll(deltaSeconds, enemies, &Enemy::scale, &Enemy::alpha)`
covers fields of a list. It stores nothing, so the list may grow and shrink. In a simulation pass 1.

**Limits.** A chain has up to four parts, held inside the tween, so it needs no allocation and a vector of enemies copies cheaply. A longer
sequence is what a clip is for. A tween that is never updated stays where it is, with no error. `auto x = enemy.scale` copies the tween,
not the number; write `float x = enemy.scale`.

**Why no `tweens.To(enemy.scale, ...)`.** A central list pointing at a value goes wrong when the value moves or dies, and in C++ nothing can
notice: a vector of enemies that grows moves them all. A saved `OnComplete` has the same problem with what it captured. Here the tween lives
and dies with its value, and code that reacts to an end runs at that moment and keeps nothing.

## Tween sets

For values that do not deserve a field, a set owns them and finds them by a key: an enum, or an object's id.

```cpp
enum class MenuPart { Title, Glow };
SecondsTweenSet<MenuPart> tweens{1.0f};                          // The value of a key not seen yet

tweens.To(MenuPart::Title, 1.4f, 0.3f, Curve::OutBack).Then(1.0f, 0.2f);
tweens.Update(deltaSeconds, [&](MenuPart finished) {             // One line; reports what ended
    if (finished == MenuPart::Title) PlaySound();
});
paint.Circle(at, 20.0f * tweens[MenuPart::Title], Fill(color));
```

The value type defaults to `float`: `SecondsTweenSet<MenuPart, Float2>` for others. The reference `To` returns is for chaining in the same
statement. A key keeps its last value until `Remove` or `Clear`, so remove the ids of objects that are gone; forgetting one costs a small
entry, not a crash. Keys are visited in order, so a run replays the same. The callback runs after everything has moved, and may change the set.

## Tracks

One value through several keys, as constant data. The caller asks at a position:

```cpp
const StepTrack<Float2> path{{0, {0.0f, 0.0f}}, {30, {80.0f, 0.0f}, Curve::InOutQuad}, {60, {80.0f, 60.0f}}};
const Float2 at = path.At(_age);
```

The curve of a key shapes the way to that key, as in `To`. Before the first key it is the first value; after the last, the last. Two keys at one
position jump there. `Curve::Hold` stays until the key, then jumps.

## Clips and poses

A pose is your own plain struct of what animates. Every field blends and reaches `State` by its own name, in declaration order; there is
no list to keep (fields are read from the struct, [`Utilities/Reflection.h`](../Core/Utilities/Reflection.h)). Its default values are the
rest pose.

```cpp
struct BossPose {
    float arm = 0.0f;
    Float2 offset{};
    Color4 tint{255, 255, 255, 255};
    int frame = 0;
};
```

Fields are numbers, enums and bools (these hold, then change at the middle of a fade), types with a `Mix(a, b, t)` such as `Float2` and
`Color4`, or other poses. A pose is a plain struct: public fields, no constructor or base class, declared outside any function and
anonymous namespace. Something that should not animate does not belong in the pose; keep it beside it. A tween of a pose
(`SecondsTween<CameraPose>`) blends every field the same way.

A clip gives some of those fields tracks, and has named moments. A field with no track stays at its default. By default the last key or moment marks
the end.

```cpp
StepClipSet<BossPose> BossClips() {
    return {
        {"idle", StepClip<BossPose>{}
                     .Track(&BossPose::arm, {{0, 0.0f}, {30, 0.3f, Curve::InOutSine}, {60, 0.0f, Curve::InOutSine}})
                     .Looping()},
        {"attack", StepClip<BossPose>{}
                       .Track(&BossPose::arm, {{0, 0.0f}, {20, -1.2f, Curve::InBack}, {30, 0.8f}})
                       .Moment("fire", 24)
                       .Length(40)},
    };
}
```

A clip set is constant, and a copy shares its data, so a thousand animators of one kind stay cheap and none can outlive what it plays. Bad data throws
`std::invalid_argument` in every configuration. Build a set in a function, as above, so the error appears when the simulation or scene is created; a
global would fail before `main`, with no message. A clip can also be used alone with your own clock: `Sample`, `ForEachCrossed`.

There is no sprite sheet here because art is drawn by code. For frames, a whole-number field works: `.Track(&BossPose::frame, {{0, 0}, {6, 1}, {12, 2}})`.

## Animators

An animator plays one clip of a set at a time and fades between them. The game decides what plays:

```cpp
StepAnimator<BossPose> _animator{BossClips(), "idle"};

void Step(const SimulationStep&) {
    if (startAttack) _animator.Play("attack", {.blend = 4, .then = "idle", .thenBlend = 8});
    _animator.Step([&](std::string_view moment) {
        if (moment == "fire") Fire();
    });
}
void State(StateVisitor& state) const { state.Field("animator", _animator); }

// The view:
const BossPose pose = simulation.Pose(runner.Alpha());
```

| `Play` option | Does |
| --- | --- |
| `.blend` | How long to fade from what was showing; 0 cuts |
| `.blendCurve` | Shapes the fade, and the fade into `then` |
| `.then`, `.thenBlend` | What plays when this clip ends, and how long to fade into it. A looping clip never ends |
| `.restart` | Plays from 0 even if it is already the current clip |

- **Playing the current clip does nothing.** So rules can say what should be showing on every step, with no bookkeeping. Use `.restart` to start over.
- **Moments reach the callback once per pass,** after the animator has moved, so the callback may call `Play`. Only the current clip meets moments; the
  one fading out does not. On a clip that ends, a moment at the very end comes with the end. On a looping clip it is the same instant as position 0 and
  is never met, so put it at 0.
- **During a fade both clips keep running.** A `Play` during a fade continues from what was showing, with no jump.
- **Asking:** `Clip()`, `Clock()`, `Finished()`, `JustFinished()`, `Blending()`. With `then`, `Clip()` already names the next clip in the step the last one ended.
  `Blending()` stays true for one step after a fade reaches its end, so a view drawing between steps reaches that end smoothly.

**In a simulation.** `State` writes which clips and where on them, so in normal play an animator adds no floats to the hash. Only a fade interrupted by
another `Play` keeps a frozen pose, because no clip could give that pose again. The pose itself is worked out, so it does not go in `State`.

**Not here.** There is no state graph: the game's rules decide, and they already live in the simulation. There are no layers: two animators can each
give part of a pose, and the game combines them. There is no playback speed on steps.

A pose can be more than a character: `CameraShot` is one, so a clip of it is an authored camera move, and a tween moves between
two shots ([Effects](Graphics/Effects.md#cinematic-camera-and-configurable-overlays)). A moment named like an effect cue can
play it ([Effects](Graphics/Effects.md#cues)).

## Curves

Each takes 0..1 and returns exactly 0 at 0 and exactly 1 at 1; a value outside 0..1 is clamped. Some leave the range on the way, on purpose. Each exists as a value,
`Curve::OutBack`, and as a function, `EaseOutBack`. `Ease(curve, t)` connects them. What stores a curve uses the value, because it can go in `State`.

| Family | Curves | Good for |
| --- | --- | --- |
| Linear | `Linear` | Bars, constant motion |
| Quad, Cubic, Quart | `In…`, `Out…`, `InOut…` | Ordinary motion, from gentle to sharp |
| Sine | `InSine`, `OutSine`, `InOutSine` | The softest: drifting, breathing |
| Expo | `InExpo`, `OutExpo` | A dash, a sudden stop |
| Back | `InBack`, `OutBack`, `InOutBack` | A wind-up; a pop-in that overshoots |
| Elastic | `InElastic`, `OutElastic` | A spring hit |
| Bounce | `InBounce`, `OutBounce` | A landing |
| Hold (value only) | `Hold` | Stays, then jumps at the end |

The same pieces work with no object at all, for a value worked out from something you already have:

```cpp
const float t = Progress01(age, 0.0f, 0.4f);
const float scale = Mix(0.5f, 1.0f, EaseOutBack(t));
```

## Timelines

A timeline is named moments on a line, as constant data. It animates nothing: it says when things happen, for a stage plan or a boss's schedule.

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
is created.

The clock is a field of the game, and it goes in `State`; the timeline does not, because it does not change. A timeline built from
`SimulationStart::parameters` is decided by the start, so it does not need to go in `State` either.

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
a moment at 45. A jump meets everything it passes, in order. Going back meets nothing. Several moments at one position are met in the order
declared. `Crossed` in `Easing.h` is older and counts the other way (`before < moment <= after`), and is meant for a single moment on a view's
clock; do not mix the two on one clock.

### Repeating

```cpp
const int cycle = attack.Wrap(_age);
attack.ForEachCrossed(cycle, cycle + 1, onMoment);
```

The last moment marks the length and is then the same instant as position 0, so it is never met. Put what happens there at 0.

### Drawing between steps

A view reads the same timeline at the simulation's clock:

```cpp
const float charge = attack.Progress("charge", "fire", cycle, alpha);
paint.Circle(position, Mix(30.0f, 60.0f, EaseOutBack(charge)), Fill(color));
```

It assumes that `cycle` is the clock the last step used. A `Wrap` makes the value jump back at the end of a cycle, on purpose. A view does not call
`ForEachCrossed` on a simulation's clock to change gameplay. To play a sound or start an effect, see
[effects are requests](Simulation.md#writing-a-simulation): a simulation asks through state the scene reads.

## Helpers for views

These advance by the seconds they are given and have no `State`, so they belong to a view or a menu. A simulation uses tweens and animators.

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

Measured in Release, per object per step, for a pose of five fields each with a track:

| Doing | Each | 5,000 per step |
| --- | --- | --- |
| `StepTween` running, elastic curve | 10 ns | 0.05 ms |
| `StepTweenSet`: step, and read by key | up to 36 ns | 0.18 ms |
| Timeline: wrap, crossing check and eased progress, 4 moments / 32 moments | 18 / 76 ns | 0.09 / 0.38 ms |
| `StepAnimator`: play, step and pose | 57 ns | 0.28 ms |
| `StepAnimator`, always fading | 80 ns | 0.39 ms |

- Sine, Expo and Elastic call the standard library's math; the others are plain arithmetic. All of them gave the same hashes in Debug
  and Release when measured, and other machines are outside the guarantee.
- These functions are in headers. A project that builds its game code with `/fp:fast` could get another copy of a curve inside its
  simulation, because the linker keeps one copy of each. Do not use `/fp:fast` in a project with a simulation. `StepSmoothed` is in
  the same position.
