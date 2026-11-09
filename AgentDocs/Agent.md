Rules for an agent working on a game built with this engine. Import this file from the project's agent file; it is
short on purpose. Paths start at the engine root (`Engine/` in a game); run tools from the project root as
`python Engine <command>`.

Plans, reports and notes for people or other agents are `.md` files in `Notes/`, not beside code.

## Reading

- Start from the matching skill below; it holds the usual example. Follow its links before searching.
- Find engine API with `python Engine api NAME` or `python Engine api WORDS` (task routes and declarations, with
  lines); then read only the printed lines. Engine `.cpp` files are not the API.
- Search only `Game/` or files you name, never all of `Engine/` or the project root.
- Before writing a list, counter, timer, hit test or other general machinery, ask `python Engine api` with the task's
  words (`python Engine api enemies that spawn and die`), not only with names you already know.
- Read files longer than about 200 lines by range.
- One task per session; when it is done, say what the next session needs and stop.
- A prompt or plan you write for another agent lists `Engine/AgentDocs/Agent.md` and the matching skills in what to
  read first, and names the engine tools you found for the task.

## Before writing your own

The engine already has these; use them instead of game code that does the same.

| When the code is about to | Use |
| --- | --- |
| keep spawned things in a vector or by index while others point at them | `Pool` (+ `PoolColliders`): `AgentDocs/Recipes/Pool.md` |
| sweep x and y one at a time, with a skin, so a character slides along walls | `MoveAndSlide`: `AgentDocs/Recipes/WalkerSlide.md` |
| remove and re-add a collider so a query does not find it | the query's `ignore` parameter: `Docs/Collision.md#many-colliders` |
| check a clip's time for a footstep, a hit frame or a muzzle flash | Animator moments: `AgentDocs/Recipes/Animator.md` |
| count steps with `age`, `phase` or `++steps` for a sequence of actions | `Timeline`: `AgentDocs/Recipes/Timeline.md` |
| keep short-lived drawings with an age, a lifetime and a cap | `Popup`, `FxStage`: `AgentDocs/Recipes/PopupEffects.md`, `AgentDocs/Recipes/FxCues.md` |
| hit-test a button by hand, or block input under a pause screen or dialog | `UiButton`, `UiLayer`: `AgentDocs/Skills/ui/SKILL.md` |
| store cells in a flat array, or pick by weight, without repeats or with pity | `Grid<T>`, `WeightedIndex`, `DrawTable`: `AgentDocs/Recipes/Procedural.md` |

## Tools

- Build with `python Engine build` (Debug, must be warning-free); test with `python Engine test` (`--label`, `--filter`).
- The developer starts the game and the editor (`python Engine run`, `python Engine editor`); do not, unless asked.
- Assume no editor is open. Use `python Engine ctl` and `python Engine refresh` only when the request is about the
  editor (an `[editor]` line, "show it in the editor"), the developer says it is open, or you were started from the
  editor's Console (`ENGINE_EDITOR_CONTROL` is set). Never probe with `ctl status`.
- An open editor is yours to drive: `ctl` opens scenes, sets values, seeks and takes screenshots; opening a scene
  launches nothing. After code changes, `refresh` rebuilds the scenes and the editor stays open.
- When the developer has turned visual checks off (`"visualChecks": false`, or a capture refused), take no more
  screenshots or frames; tell the developer what to look at.
- Started from the editor's Console, report progress on longer tasks with `python Engine ctl note TEXT`; add
  `--state waiting` when you need the developer and `--state idle` when done.

## Skills

Read the matching skill before acting. Each one is a short procedure; follow its links only when needed.

| When the request is to | Read |
| --- | --- |
| diagnose a game, worker or native test crash; a `Crash report:` path | `AgentDocs/Skills/crash/SKILL.md` |
| show, open, look at, inspect or explain something in the editor; an `[editor] ...` line | `AgentDocs/Skills/editor/SKILL.md` |
| draw a new character, enemy, item, prop, icon or other standalone graphic | `AgentDocs/Skills/graphic-asset/SKILL.md` |
| change something visual or gameplay-facing and check it | `AgentDocs/Skills/visual-change/SKILL.md` |
| fix a failed build, test or refresh, or warnings | `AgentDocs/Skills/build-fix/SKILL.md` |
| write code against the engine API (find headers, recipes, concepts) | `AgentDocs/Skills/api/SKILL.md` |
| check, compare, clean up or play a sound; find unused or missing sounds | `AgentDocs/Skills/audio/SKILL.md` |
| test gameplay without a window, reproduce a replay, write gameplay rules as a simulation | `AgentDocs/Skills/simulation/SKILL.md` |
| add or debug hits, overlaps, hit boxes, walls, lasers, fast bullets; "goes through", "did not hit" | `AgentDocs/Skills/collision/SKILL.md` |
| build or debug game UI: menus, options, pause, dialogs, panels, HUD, a widget of its own; "does not react", "overlaps" | `AgentDocs/Skills/ui/SKILL.md` |
| add or debug animation and timing: tweens, easing, character clips and fades, boss patterns, wave plans; "does not move", "fires late", "fires twice" | `AgentDocs/Skills/animation/SKILL.md` |

## Hard invariants

- New standalone graphics go in `Game/Assets/Graphics/<Folder>/X.h`, `X.cpp` and `X.asset.cpp`; model-reading Views stay in place, variants use `items`, existing code need not move, and project agent rules take precedence: `Docs/Editor/Api.md#graphic-assets`.
- Draw through Painter: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Use logical canvas coordinates, y down: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Angles are radians, clockwise: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Keep Painter/scopes inside the drawing call; end scopes in reverse order: `Docs/Graphics/Painter.md#scopes`.
- Game code includes no engine Internals or raylib: `Docs/Contributing/ApiBoundaries.md#three-tiers`, `Docs/AssetsAndPlatforms.md#platform-responsibilities`.
- Do not keep a decorative View clock; use paint.Time(): `Docs/Runtime.md#input-and-time-selection`.
- Pass real frame duration through TimeScale; scenes select their time domain: `Docs/Runtime.md#input-and-time-selection`.
- Simulation code stays free of drawing, audio, clocks and `<random>`; everything deciding gameplay comes from its start or its steps: `Docs/Simulation.md#writing-a-simulation`.
- Collision goes through `Physics/Collision`, never Box2D. The game owns positions and calls `Move`; fast objects use `Sweep`: `Docs/Collision.md#many-colliders`.
- Keep registered objects alive; shut scenes down before members die: `Docs/Runtime.md#lifecycle`.
- Own surfaces in the scene/View, before renderer teardown: `Docs/Graphics/Painter.md#images-and-surfaces`.
