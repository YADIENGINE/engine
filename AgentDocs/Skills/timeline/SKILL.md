---
name: timeline
description: Add or debug timing and value animation - boss attack patterns, wave plans, cooldown phases, intros, tweens, easing, "fires a step late", "fires twice", "never fires". Uses named moments on a timeline with a clock the game owns. Not for following a moving target (SpringPoint).
---

# Add or debug timing

Reference: `Docs/Animation.md`. Example that compiles: `AgentDocs/Recipes/Timeline.md`. Includes: `Animation/Timeline.h`,
`Animation/Easing.h`; a simulation needs no CMake line.

## Choose the call

| The request is | Use |
| --- | --- |
| Something happens at a moment | `timeline.ForEachCrossed(age, age + 1, fn)` before `++age` |
| A value moves between two moments | `Mix(a, b, EaseOutCubic(timeline.Progress("from", "to", age)))` |
| Which part of the sequence is it in? | `timeline.PhaseAt(age)` |
| It repeats | `const int cycle = timeline.Wrap(age);`, then ask with `cycle` |
| The view draws it smoothly between steps | `timeline.Progress("from", "to", cycle, runner.Alpha())` |
| One value over a fixed time, no names | `Mix(a, b, EaseOutBack(Progress01(age, 0.0f, 0.4f)))` |

## Write it

1. In a simulation use `StepTimeline`, with whole steps. If the design is in seconds, write `StepsIn(1.5, 60)`. Use
   `SecondsTimeline` only in a view or a menu.
2. Keep the timeline as a member, not a global: bad data then throws when the simulation is created, with a message.
3. Keep the clock as an `int` field and add it to `State`. Do not add the timeline: it does not change.
4. Name the moments for tools: override `Moments` with `AppendMoments` from `Simulation/TimelineMoments.h`. Give a second
   timeline a prefix.
5. Test without a window: the [simulation skill](../simulation/SKILL.md). Check the moment one step before and one step after.

## Pitfalls

- **Moving the clock first** skips a moment at 0 and meets the others one step early. Ask, then `++age`.
- **`Wrap` never meets the last moment:** it marks the length, and it is the same instant as 0. Put that action at 0.
- **A typo in a name throws** when the line runs, so a branch that rarely runs hides it. A scenario should reach each moment.
- **`moment == "fire"` with a misspelled name never matches and does not throw.** Check that a scenario sees the effect.
- **Do not use `float` seconds for a clock in a simulation:** positions in a run are steps.
- **A view must not change gameplay** on a moment. The rules do that; a view only reads.
- `PhaseAt(age)` after a step already names the moment that the next step will meet.

## A moment that goes wrong

1. **Reproduce:** a scenario with `read` of the clock and the field the moment changes, one step either side.
2. **Look at it:** `python Engine ctl status` lists `moments`. `python Engine ctl seek --moment NAME` pauses there, and
   `python Engine ctl step --sim` advances one step at a time.
3. **A moment missing from `moments`:** its start is not decided yet at this step, or `Moments` does not list that timeline.
4. **It fires twice or not at all:** check that the clock moves once per step and that nothing else resets it. A `Wrap` used in
   one place and a raw clock in another disagree after the first cycle.

Report what you checked, and add the scenario as a test.
