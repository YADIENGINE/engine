# Project setup

The engine is a separate git repo, and each game uses it as a submodule named `Engine/` at its root. Game code, editor
previews, and documentation stay in the game's repo; the engine repo sees no games.

```text
MyProject/                  (game repo)
  Engine/                   ← submodule: engine repo
    Core/  Editor/  Docs/  Tools/  ThirdParty/  __main__.py  ...
  Game/                     game sources + Assets/ (.cpp files found automatically) + Distribution/ (notices sent
                            to players: GameNotices.txt, credits; see Release.md)
  Editor/                   project preview worker (EngineEditor previews; .cpp files found automatically)
  CMakeLists.txt  CMakePresets.json
  Project.json              project settings read by engine tools
```

There are no engine scripts at the project root; run all commands from the project root with `python Engine <command>`
(list and contract: [ToolsContract](ToolsContract.md)).

The include root is `Engine/Core/` (`#include "Runtime/Game.h"`); editor API includes are relative to the engine root
(`#include "Editor/Api/..."`). The game also adds its own root (`Game/`).

## New project

Requirements: on Windows, VS 2022 Build Tools (C++ workload + "C++ CMake tools", which includes Ninja), CMake 3.24+, and
Python 3.10+. Dependencies (raylib, JSON, editor and test libraries) come in the engine's `ThirdParty/` folder; building
does not require internet access.

Place only Engine in the project folder; the folder must be named exactly `Engine` (GitHub ZIPs extract as
`<repo>-main`; rename it). Then run setup from the project folder:

```powershell
cd MyProject
python Engine new                  # asks for the project name (default: folder name)
python Engine new --name MyGame    # without prompting
python Engine new --dry-run        # lists what it would create without writing
```

`new` writes `Engine/Tools/ProjectTemplate/` into the project folder: `CMakeLists.txt`, `CMakePresets.json`,
`Project.json`, `.gitignore`, `.gitattributes`, `Game/Assets/`, `Editor/`, `Game/Distribution/GameNotices.txt`. The
project name is written to `project(...)` and the `Project.json` `"name"` field; the executable, Release package, and
editor worker (`<name>Preview`) derive from this name. The name starts with a letter and contains only letters/digits/`_`.
Existing files are not overwritten (skipped files are listed); if `Project.json` exists at the root, the project is
considered set up and nothing is done. Finally, it reports missing tools (Python, CMake, Visual Studio C++ tools,
Ninja). It does not touch Git; to keep Engine as a submodule, see [git](#git) below.

```powershell
python Engine run       # build Debug + run
python Engine editor    # EngineEditor + project worker
python Engine test      # headless tests (Engine tests)
```

`Game/` and `Editor/` start empty (`Game/Assets/` and `.gitkeep`). While empty, `engine_add_game` and
`engine_add_editor_worker` build the engine's starter game and starter worker: the game opens the "Game/ has no sources
yet" screen, and the editor opens the Engine → Starter preview. When the first `.cpp` is added, CMake reconfigures and
the project's code takes over: [First game](GettingStarted.md), [First editor preview](Editor/GettingStarted.md).

## Project.json

The only place engine tools identify the project. Paths are relative to the project root; preset names are from the
project's `CMakePresets.json`. Only used sections are required (e.g. no `editor` in a project without an editor). The
example below shows all fields; the template omits `target`, `release.name`, and `editor.worker`, whose defaults come
from `name`.

```json
{
  "version": 1,
  "name": "MyProject",
  "hooks": "ProjectHooks.py",
  "game": {
    "target": "MyProject",
    "buildDirectory": "build",
    "configurePreset": "desktop",
    "buildPresets": { "Debug": "debug", "Release": "release" }
  },
  "release": {
    "directory": "Release", "name": "MyProject",
    "notices": "Game/Distribution/GameNotices.txt", "files": ["Game/Distribution/AudioCredits.txt"]
  },
  "tests": {
    "buildDirectory": "build/tests",
    "configurePreset": "tests", "buildPreset": "tests-debug", "testPreset": "tests-debug"
  },
  "editor": {
    "buildDirectory": "build/editor",
    "worker": "MyProjectPreview",
    "cacheVariables": { "APP_BUILD_GAME": "OFF", "APP_BUILD_EDITOR": "ON" }
  }
}
```

| Field | Used by | Meaning |
| --- | --- | --- |
| `game.target` | `build`, `run` | `engine_add_game` target (default `name`); executable `buildDirectory/<Config>/<target>.exe` |
| `game.buildDirectory` | `build`, `run` | must match the configure preset's `binaryDir` |
| `release` | `release` | `directory/name/` (default `Release/<name>/`) and adjacent ZIP: executable + `Assets` + generated `ThirdPartyNotices.txt` + `files`; [Release](Release.md) |
| `tests` | `test` | test presets; result `buildDirectory/reports/<run-id>/results.json` |
| `editor.worker` | `editor` | `engine_add_editor_worker` target (default `<name>Preview`) |
| `editor.cacheVariables` | `editor` | project options added to the editor build (such as disabling the game and enabling the worker) |
| `hooks` | `release` | optional Python file (below) |

`editor` supplies `ENGINE_BUILD_RUNTIME/EDITOR=ON`, `ENGINE_BUILD_TESTS/EDITOR_TESTS=OFF`, and
`CMAKE_CONFIGURATION_TYPES=Debug;Release` itself. The editor is configured in a separate build folder and does not touch
the game's build.

### Hooks

Project-specific work that does not fit JSON lives in known functions in the `hooks` file:

| Function | When | Returns |
| --- | --- | --- |
| `package_assets(root: Path) -> list[str]` | `python Engine release` | paths relative to `Assets/` to include (folders or files); if absent, `Assets/` is copied as-is |

For example, a game can read its audio catalog from a source file and package only sounds in use.

## Tools (Engine/Tools)

Commands and options: [ToolsContract](ToolsContract.md). Python package, standard library only:

| Module | Job |
| --- | --- |
| `cli.py` | `python Engine <command>`: arguments, project root, error → exit code |
| `project.py`, `manifest.py` | `Project.json` (including hook loader), `EngineManifest.json` |
| `msvc.py` | MSVC environment (vcvars64); cached in `build/.msvc-env.json`, refreshed when VS updates |
| `cmake.py` | command execution, CMake cache, safe build folder removal |
| `commands/` | `build.py` (build, run), `test.py` (structured report), `editor.py`, `release.py` ([Release](Release.md)), `new.py` |
| `include_boundaries.py` | `check`: Internals boundary ([API boundaries](Contributing/ApiBoundaries.md)) |
| `ProjectTemplate/` | new project files; `@PROJECT_NAME@` is replaced by the name, `gitignore`/`gitattributes` are written with dots |

The tools' own tests require no compiler: `python -m unittest discover -s Tools/Tests -t .` at the engine root.

## Git

`new` does not touch git. To keep the game in git and connect Engine as a submodule, run at the project root after
setup:

```powershell
git init
git submodule add <ydarcadeengine-url> Engine   # registers Engine/ if it is already a clone of this URL
git config -f .gitmodules submodule.Engine.branch main
```

If Engine came from a ZIP, first remove `Engine/` and obtain it again with `submodule add` above; or commit Engine as a
plain folder with the game (updates are then copied manually).

## Updating and working on the engine

- Get the latest engine: `git submodule update --remote Engine`, then commit the `Engine` pointer in the game's repo.
  Use `git clone --recurse-submodules` when cloning, or `git submodule update --init` afterwards.
- Engine changes: first commit and push on a branch inside `Engine/` (`git -C Engine switch main`), then commit the
  pointer in the game's repo. One commit cannot write to both repos; the game's commit must always point to a pushed
  engine commit.
- Test the engine alone (without a game): `python . test` at the engine repo root (it has its own `Project.json` and
  `CMakePresets.json`; details in [Testing](Contributing/Testing.md)).