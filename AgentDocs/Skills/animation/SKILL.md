---
name: animation
description: Add or debug animation and timing - tweens, easing, a value that moves, character clips and fades, walk and attack animations, poses, boss attack patterns, wave plans, phases, "fires a step late", "fires twice", "never fires", "it does not move", "it jumps". Tweens, animators, clips and timelines; all draw nothing and work inside a simulation or a view.
---

# Add or debug animation

Reference: `Docs/Animation.md`. Examples that compile: `AgentDocs/Recipes/Tween.md`, `AgentDocs/Recipes/Animator.md`,
`AgentDocs/Recipes/Timeline.md`. Includes start at `Animation/`; a simulation needs no CMake line.

## Choose the part

| The request is | Use |
| --- | --- |
| A value should go somewhere: pop-in, recoil, cursor, fade | A tween as a field: `SecondsTween<float> scale{1.0f};` |
| The same, with no field worth adding | `SecondsTweenSet<MenuPart> tweens{1.0f};`, found by key |
| One value through several fixed points | `StepTrack<Float2> path{{0, a}, {30, b}, {60, c}};`, `path.At(age)` |
| A character with named animations and fades | A pose struct, a clip set, an animator |
| Something happens at a fixed time in a plan | `StepTimeline`, `ForEachCrossed(age, age + 1, fn)` before `++age` |
| Which part of the plan is it in? | `timeline.PhaseAt(age)` |
| A follow-the-target motion | `SpringPoint` |

**Inside a simulation use `Step…` names, with whole steps and `Step()`. In a view or a menu use `Seconds…` names, with `Update(deltaSeconds)`.**
If the design is in seconds, write `StepsIn(1.5, 60)` in a simulation.

## Write it

1. **A tween** is the value. `scale.To(1.3f, 0.1f, Curve::OutQuad).Then(1.0f, 0.2f);`, then read it like a float. One `Update` per frame:
   `UpdateAll(deltaSeconds, a, b)` or `UpdateAll(deltaSeconds, enemies, &Enemy::scale)`. React to an end with `if (scale.JustFinished())`.
2. **A pose** is a plain struct whose defaults are the rest pose, with a `Fields()` listing every field.
3. **Clips** go in a function returning a `StepClipSet<Pose>`, not a global: bad data then throws when the simulation is created, with a message.
4. **An animator** is built from the set and a first clip. Each step, say what should show and advance:
   `_animator.Play("walk", {.blend = 8});`, then `_animator.Step(onMoment);`. Calling `Play` with the current clip does nothing.
5. **In a simulation**, write state: `state.Group("animator", [&] { _animator.State(state); });`, the same for each tween and tween set. Timelines,
   clips and poses are constant or worked out, so they stay out. A timeline's clock is a field and goes in.
6. **In the view**, draw between steps with `_animator.Pose(alpha)`, `tween.Value(alpha)`, `timeline.Progress("from", "to", cycle, alpha)`, with
   `alpha` from `runner.Alpha()`.
7. **Name moments for tools:** override `Moments` with `AppendMoments` from `Simulation/TimelineMoments.h`.
8. Test without a window: the [simulation skill](../simulation/SKILL.md). Check a moment one step before and one step after.

## Pitfalls

- **A tween never updated stays where it is, with no error.** So does an animator. This is the first thing to check for "it does not move".
- **`auto x = enemy.scale` copies the tween.** Write `float x = enemy.scale`.
- **There is no `tweens.To(enemy.scale, ...)`, on purpose.** Do not add a pointer to a value in a list: a vector that grows moves them all.
- **Do not store a pose or a `Play` result in `State`.** Write the animator; the pose is worked out from it.
- **A field missing from `Fields()` does not blend and does not reach `State`.** It compiles, and hashes cannot show it.
- **Moving a timeline's clock first** skips a moment at 0 and meets the others one step early. Ask, then `++age`.
- **`Wrap` and looping clips never meet a moment at the very end:** it is the same instant as 0. Put that action at 0.
- **`moment == "fire"` with a misspelled name never matches and does not throw.** A scenario should see each moment's effect.
- **A chain has four parts at most.** A longer sequence should be a clip.
- **A `Play` every step needs `.restart = true` only to start over;** with it on every step, the clip never gets past 0.
- **Do not use `float` seconds for a clock in a simulation,** and a view must not change gameplay on a moment. The rules do that; a view only reads.

## Something that goes wrong

1. **Reproduce:** a scenario with `read` of the clock and the field the moment changes, one step either side.
2. **Read it:** `python Engine sim state TAPE --step N` lists `animator.clip`, `animator.clock`, `animator.blend.clock`, and for a tween `value`, `clock`, `index`.
3. **Look at it,** only with an open editor (`AgentDocs/Agent.md`): `python Engine ctl status` lists `moments`. `python Engine ctl seek --moment NAME` pauses there, and `python Engine ctl step --sim`
   advances one step at a time.
4. **A moment missing from `moments`:** its start is not decided yet at this step, or `Moments` does not list that timeline.
5. **It fires twice or not at all:** check that the clock moves once per step and that nothing else resets it. `Step` called twice in a step runs the animation at double speed.
6. **It jumps:** a cut where a fade was meant (`.blend`), or a `Snap`. A `To` never jumps.

Report what you checked, and add the scenario as a test.
