Rules for an agent working on a game built with this engine. Import this file from the project's agent file; it is
short on purpose. Paths start at the engine root (`Engine/` in a game); run tools from the project root as
`python Engine <command>`.

## Tools

- Build with `python Engine build` (Debug, must be warning-free); test with `python Engine test` (`--label`, `--filter`).
- The developer starts the game and the editor (`python Engine run`, `python Engine editor`); do not, unless asked.
- An editor that is already open is yours to drive: `python Engine ctl` opens scenes, sets values, seeks and takes
  screenshots. Opening a scene is not launching anything. `python Engine ctl status` exits 2 when no editor is open.
- After code changes while the editor is open, run `python Engine refresh`; the editor stays open.
- Started from the editor's Console (`ENGINE_EDITOR_CONTROL` is set), report progress on longer tasks with
  `python Engine ctl note TEXT`; add `--state waiting` when you need the developer and `--state idle` when done.

## Skills

Read the matching skill before acting. Each one is a short procedure; follow its links only when needed.

| When the request is to | Read |
| --- | --- |
| show, open, look at, inspect or explain something in the editor; an `[editor] ...` line | `AgentDocs/Skills/editor/SKILL.md` |
| change something visual or gameplay-facing and check it | `AgentDocs/Skills/visual-change/SKILL.md` |
| fix a failed build, test or refresh, or warnings | `AgentDocs/Skills/build-fix/SKILL.md` |
| write code against the engine API (find headers, recipes, concepts) | `AgentDocs/Skills/api/SKILL.md` |
| check, compare, clean up or play a sound; find unused or missing sounds | `AgentDocs/Skills/audio/SKILL.md` |

## Hard invariants

- Draw through Painter: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Use logical canvas coordinates, y down: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Angles are radians, clockwise: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Keep Painter/scopes inside the drawing call; end scopes in reverse order: `Docs/Graphics/Painter.md#scopes`.
- Game code includes no engine Internals or raylib: `Docs/Contributing/ApiBoundaries.md#three-tiers`, `Docs/AssetsAndPlatforms.md#platform-responsibilities`.
- Do not keep a decorative View clock; use paint.Time(): `Docs/Runtime.md#input-and-time-selection`.
- Pass real frame duration through TimeScale; scenes select their time domain: `Docs/Runtime.md#input-and-time-selection`.
- Keep registered objects alive; shut scenes down before members die: `Docs/Runtime.md#lifecycle`.
- Own surfaces in the scene/View, before renderer teardown: `Docs/Graphics/Painter.md#images-and-surfaces`.
