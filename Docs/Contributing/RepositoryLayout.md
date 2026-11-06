# Repository layout

## Source and targets

| Path | Responsibility |
| --- | --- |
| `Core/` | Shipping engine code and include root (`Runtime/Game.h`); targets in `Core/CMakeLists.txt` |
| `Core/Runtime/` | Game, settings, loop and services; `Core/Main.cpp` calls the project's `CreateGame()` |
| `Core/GameObjects/`, `Core/Scene/` | Headless object/scene lifecycle and optional object ownership |
| `Core/Graphics/` | Paint, Text, Surface, Post, Light; GPU/render implementation; Draw and Style helpers |
| `Assets/Graphics/` | `EngineAssets`: ready-made graphics with their own look, plus their editor registrations; include root `Assets/` |
| `Assets/Audio/` | Ready-made sounds as files, copied beside every game as `Assets/Engine/Audio`; `release` packages those a game uses |
| `Core/Ui/` | Game UI: Layout, Kernel, Looks, Widgets, Kit, Sound ([UI](../Ui.md)) |
| `Core/Input/`, `Core/Audio/`, `Core/Platform/` | Public services and private platform backends |
| `Core/Fx/`, `Core/Gameplay/`, `Core/Progress/` | Headless effects and reusable gameplay/view logic |
| `Core/Stage/` | A scene's view side in one fixed order (`Stage`, `StageView`, `DepthOrder`), in `EngineHeadless`; view code that never reaches a simulation ([Stage](../Graphics/Stage.md)) |
| `Core/Utilities/` | Foundation types and helpers, grouped under `Geometry/`, `Math/` and `Random/`. `Random/` (seeded numbers, tables, scattering, noise) builds as `EngineRandom`, with precise floats; the other `.cpp` files belong to `EngineHeadless` |
| `Core/Diagnostics/` | Cross-cutting development checks and frame instrumentation contracts |
| `Core/Diagnostics/Internals/` | `EngineDiagnostics` process capture and the separate `EngineCrashReporter`; no window or game dependency |
| `Core/Options/` | Engine/player options, backed by Persistence |
| `Core/Tuning/` | Tuning format, live values (`ENGINE_TUNING_LIVE`), `Tuning<T>()` |
| `Core/Animation/` | Values over time: tweens, clips, animators, timelines and curves for simulations and views, small helpers for views. Headers only, with no target of their own; nothing here draws |
| `Core/Camera/` | Game cameras (`EngineCamera`): `Camera2D`, `Cameras`, `CameraView`; math only, so a simulation may keep one. `Internals/` holds the follow rules; `Debug/` draws and belongs to `EngineHeadless` |
| `Core/Procedural/` | Generated layouts (`EngineProcedural`): rooms, random walks, caves, floor regions over `Grid<MapCell>`; includes only `Utilities/`, so a simulation may generate its level ([Procedural](../Procedural.md)) |
| `Core/Physics/` | Collision over Box2D (`EnginePhysics`); Box2D stays behind `Internals/`. `Debug/` draws and belongs to `EngineHeadless` |
| `Core/Modules/` | Opt-in systems; each module owns its facade, internals, documentation and tests |
| `Core/Tests/` | Core tests and shared `Support/` (temporary directories, diagnostics, gzip inflater) |
| `ToolUi/` | `EngineToolUi`: shared ImGui context, fonts, theme and widgets for tools; never shipped in games |
| `Terminal/` | `EngineTerminal`: ConPTY/libvterm session; `EngineTerminalUi`: ImGui terminal view; tools only |
| `Editor/` | Editor shell, public scene catalog API and worker runtime; root-relative includes (`Editor/Api/...`) |
| `ThirdParty/` | Unmodified vendored dependencies, notices and notice tests; see [Third-party rules](../../ThirdParty/README.md) |
| `Tools/` | Standard-library Python CLI and project template (offline audio authoring has separate NumPy/SciPy dependencies) |

`EngineHeadless` contains platform-free code; `Engine` adds the runtime platform layer. `EngineAudio` is the audio engine over miniaudio: it needs no window, so `Engine` links it and its tests run it offline. `EngineSimulation`,
`EnginePhysics`, `EngineCamera`, `EngineProcedural`, `EngineRandom` and `EngineTuning` sit below `EngineHeadless` and reach neither drawing nor audio, so
simulation code can link only them. `EngineAssets` sits above `EngineHeadless`; games, workers and tests link it, and
nothing in Core does.

`Core/Utilities/` accepts only headless foundation APIs with no clearer domain owner. Geometry, scalar/index math
and everything random (generators, tables, scattering, noise) use their named subfolders; UI, camera, options and diagnostics types stay with those owners.
Do not add `Common`, `Misc` or another catch-all layer beneath it.

Create consuming targets with `engine_add_game`, `engine_add_simulation` and `engine_add_editor_worker`.
Add each new `.cpp` to its target. Module targets and testing rules live in
[Module rules](ModuleRules.md#1-directory-structure) and [Testing](Testing.md#adding-tests).
Graphics dependency directions and ownership have one source:
[Architecture](../Architecture.md#graphics-layout).

## Tools layout

| Path | Responsibility |
| --- | --- |
| `__main__.py`, `Tools/cli.py` | `python Engine <command>`; argument parsing and dispatch |
| `Tools/project.py`, `Tools/manifest.py` | Project settings/hooks and engine identity/notes |
| `Tools/msvc.py`, `Tools/cmake.py`, `Tools/errors.py` | MSVC environment, shared CMake operations, exit codes |
| `Tools/commands/` | Build/run, tests/reports, editor, release, setup and refresh |
| `Tools/include_boundaries.py` | Public/private include enforcement; recipes are consumers |
| `Tools/tuning/` | Tuning generation, validation and canonical formatting; shared cases in `Tools/Tests/TuningCases` |
| `Tools/agentdocs/` | Public declaration extraction for `api`, snippet synchronization into recipes and skills, AgentDocs checks |
| `Tools/AudioGeneration/` | Offline audio authoring; public import `AudioGeneration.SoundGenerator`; recipes/assets belong to callers |
| `Tools/ProjectTemplate/` | Files copied by `new`; `@PROJECT_NAME@` substitution; dotless gitignore/gitattributes |
| `Tools/Tests/` | Compiler-free unittest suite, also registered under CTest label `engine` |

Tuning's Python and C++ validators change together using their shared fixture cases.
MSVC discovery caches its environment in `build/.msvc-env.json` and refreshes it when Visual Studio changes.
Empty project source folders use `Core/Runtime/Starter/StarterGame.cpp` and `Editor/Worker/StarterWorker.cpp`;
examples belong in documentation, not the project template.

## Documentation ownership

| Location | Source of truth |
| --- | --- |
| `Docs/` | Human concepts, architecture and guides |
| `Core/Modules/<Module>/` | Module supported API (`README.md`), planned work (`Roadmap.md`) and extra guides |
| `AgentDocs/` | Agent rules, skills, task routes (`Tasks.json`) and compiled recipes |
| `AGENTS.md` (`CLAUDE.md` imports it) | Short contributor rules; excluded from launcher copies |
| `Docs/Design/` | Historical decisions/plans; excluded from launcher copies |

Public declarations are not copied into AgentDocs: `python Engine api` reads the headers on every call, so a
declaration's comment is what agents see. New public system or plausible agent task → add a route to
`AgentDocs/Tasks.json`; `check` validates its files, recipe and doc headings.
Recipe API change → keep the recipe compiling in the engine test build and resync its Markdown snippet with
`check --fix`. Recipe sources are consumers: no engine internals or raylib includes.
AgentDocs never explains a concept already explained in Docs; link to its section instead.
The extraction is heuristic (no preprocessing); headers remain authoritative. Unsupported declarations are counted;
undocumented declarations are reported separately by `python -m Tools.agentdocs`. API names written in backticks in
AgentDocs prose must exist in a public header or recipe code; `check` reports the rest.
The project template gets no `CLAUDE.md`, `AGENTS.md` or AI setting; developers opt in from their own agent file.
`AgentDocs/` ships in launcher copies and must not be export-ignored.
