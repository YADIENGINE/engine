Rules for an agent working on a game built with this engine. Import this file from the project's agent file; it is
short on purpose. Paths start at the engine root (`Engine/` in a game); run tools from the project root as
`python Engine <command>`.

## Reading

- Start from the matching skill below; it holds the usual example. Follow its links before searching.
- Search only `Game/` or files you name, never all of `Engine/` or the project root. Engine `.cpp` files are not the
  API: headers and `AgentDocs/Api/` are.
- Read files longer than about 200 lines by range.
- One task per session; when it is done, say what the next session needs and stop.

## Tools

- Build with `python Engine build` (Debug, must be warning-free); test with `python Engine test` (`--label`, `--filter`).
- The developer starts the game and the editor (`python Engine run`, `python Engine editor`); do not, unless asked.
- Assume no editor is open. Use `python Engine ctl` and `python Engine refresh` only when the request is about the
  editor (an `[editor]` line, "show it in the editor"), the developer says it is open, or you were started from the
  editor's Console (`ENGINE_EDITOR_CONTROL` is set). Never probe with `ctl status`.
- An open editor is yours to drive: `ctl` opens scenes, sets values, seeks and takes screenshots; opening a scene
  launches nothing. After code changes, `refresh` rebuilds the scenes and the editor stays open.
- Started from the editor's Console, report progress on longer tasks with `python Engine ctl note TEXT`; add
  `--state waiting` when you need the developer and `--state idle` when done.

## Skills

Read the matching skill before acting. Each one is a short procedure; follow its links only when needed.

| When the request is to | Read |
| --- | --- |
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
