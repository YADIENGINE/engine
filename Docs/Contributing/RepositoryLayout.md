# Repository layout

## Source and targets

| Path | Responsibility |
| --- | --- |
| `Core/` | Shipping engine code and include root (`Runtime/Game.h`); targets in `Core/CMakeLists.txt` |
| `Core/Runtime/` | Game, settings, loop and services; `Core/Main.cpp` calls the project's `CreateGame()` |
| `Core/GameObjects/`, `Core/Scene/` | Headless object/scene lifecycle and optional object ownership |
| `Core/Graphics/` | Paint, Text, Surface, Post; GPU/render implementation; Draw, Icons, Ui, Style, Anim helpers |
| `Core/Input/`, `Core/Audio/`, `Core/Platform/` | Public services and private platform backends |
| `Core/Fx/`, `Core/Menus/`, `Core/Gameplay/`, `Core/Progress/`, `Core/Utilities/` | Headless effects and reusable logic/value types |
| `Core/Options/` | Engine/player options, backed by Persistence |
| `Core/Tuning/` | Tuning format, live values (`ENGINE_TUNING_LIVE`), `Tuning<T>()` |
| `Core/Modules/` | Opt-in systems; each module owns its facade, internals and tests |
| `Core/Tests/` | Core tests and shared `Support/` (temporary directories, diagnostics, gzip inflater) |
| `ToolUi/` | `EngineToolUi`: shared ImGui context, fonts, theme and widgets for tools; never shipped in games |
| `Terminal/` | `EngineTerminal`: ConPTY/libvterm session; `EngineTerminalUi`: ImGui terminal view; tools only |
| `Editor/` | Editor shell, public editor scene API and worker runtime; root-relative includes (`Editor/Api/...`) |
| `ThirdParty/` | Unmodified vendored dependencies, notices and notice tests; see [Third-party rules](../../ThirdParty/README.md) |
| `Tools/` | Standard-library Python CLI and project template (offline audio authoring has separate NumPy/SciPy dependencies) |

`EngineHeadless` contains platform-free code; `Engine` adds the runtime platform layer.
Create consuming targets with `engine_add_game` and `engine_add_editor_worker`.
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
| `Tools/agentdocs/` | Deterministic public API extraction and recipe snippet synchronization |
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
| `AgentDocs/` | Task routing, compiled recipes and generated public API signatures |
| `AGENTS.md` (`CLAUDE.md` imports it) | Short contributor rules; excluded from launcher copies |
| `Docs/Design/` | Historical decisions/plans; excluded from launcher copies |

Public API change → regenerate `AgentDocs/Api/` with `python . check --fix`; `check` rejects stale output.
New public system or plausible agent task → add a row in `AgentDocs/Index.md`.
Recipe API change → keep the recipe compiling in the engine test build and resync its Markdown snippet with
`check --fix`. Recipe sources are consumers: no engine internals or raylib includes.
AgentDocs never explains a concept already explained in Docs; link to its section instead.
Generated pages are heuristic navigation aids; headers remain authoritative. Unsupported declarations are counted;
undocumented declarations are reported separately by `python -m Tools.agentdocs`.
The project template gets no `CLAUDE.md`, `AGENTS.md` or AI setting; developers opt in from their own agent file.
`AgentDocs/` ships in launcher copies and must not be export-ignored.
