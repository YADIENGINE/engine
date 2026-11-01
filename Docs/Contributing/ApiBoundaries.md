# API boundaries

A game developer should see only what they are meant to use. Everything else lives in an `Internals/` folder
next to the system it belongs to. There is no engine-wide `Internals/` folder: a private file stays beside its
public facade so that ownership is obvious from the path.

## Three tiers

| Tier | Location | May be included by |
| --- | --- | --- |
| Public | any engine path without an `Internals/` segment | everyone, games included |
| Engine-private | core `X/Internals/` (`Input/Internals/`, `Audio/Internals/` ...) | any code in the engine repository: core, modules, editor, engine tests |
| Owner-private | `Modules/X/Internals/`, `Editor/Internals/`, `Terminal/Internals/` | only `Modules/X/` (its tests included), `Editor/` or `Terminal/` |

Engine-private exists because core subsystems implement each other: the runtime drives the input reader, the
editor host feeds input samples, modules use core file storage (`Platform/Internals/`). A folder with no public
facade simply keeps implementation under `Internals/`. Owner-private keeps modules independent
([Module rules](ModuleRules.md)): a module never reaches into another module's internals, and the core never
reaches into a module's internals.

A game's own `Internals/` folders are the game's business; these rules cover only engine headers.

## What goes into `Internals/`

A header moves into `Internals/` when the game has no reason to name it: implementation helpers, backends,
platform glue, instrumentation the engine wires up itself. Public platform actions use `Platform/Shell.h`
(`OpenExternalUrl`, `CopyToClipboard`); their backend remains under `Platform/Internals/`.

Two things keep a header public even if no game includes it directly:

- **It is part of a public signature.** A type that appears as a parameter, return value or public member of a
  public header (`GameSettings`, `FloatRange`, `ButtonAnim` ...) is public. Moving it would only hide the name,
  not the dependency.
- **A public header includes it.** A header reachable through a public header is effectively public. Hide it
  first: forward-declare it and own it through `std::unique_ptr` (the way `Game` holds `Renderer`, `SceneManager` and
  `InputNormalizer`), or
  move the include into the `.cpp`. Only then move the file.

Development tooling (frame observers, execution overrides used by the editor and profiler) is public API for
tools, not internal; do not hide it from the editor.

## Facades

A system with internals has a small public facade beside its `Internals/` folder (`Input/Input.h`,
`Audio/AudioSystem.h`, `Modules/X/X.h`). All public headers use the single `.h` extension.

## Enforcement

`AgentDocs/Recipes/Code/` is consumer code even inside the engine repository: it may not include any engine
`Internals/` header or raylib platform header. Recipes compile in the engine test build.

`Tools/include_boundaries.py` checks every `#include "..."` in the source tree (the game project when the engine is
a submodule) against the table above. It runs as the CTest `Engine.IncludeBoundaries` (label `engine`), so
`python Engine test --label engine` fails on a violation. Run it alone without a compiler:

```powershell
python Engine check .
```
