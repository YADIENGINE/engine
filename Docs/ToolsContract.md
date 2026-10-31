# Tools contract

All engine tools share one command line. The launcher (and any automation: AI agents, CI) communicates with the engine
only through the commands in this document; the launcher runs tools from the project's own `Engine/`, so projects using
different engine versions work side by side. Everything outside this document (module names, internal functions, output
text) can change freely.

## Invocation

```powershell
python Engine [--project ROOT] <command> [options]      # from the project root
python . [--project ROOT] <command> [options]           # from the engine repo root
```

Python runs the `Engine` folder directly because it contains `__main__.py`. Python 3.10+, standard library only. Tools
also work with an isolated interpreter (`python -I`, the launcher's bundled Python with `._pth`): they do not rely on
the working folder being in `sys.path` and pass their own interpreter to CMake (`Python3_EXECUTABLE`), so build steps
use the same Python. Project root: `--project` if supplied; otherwise the engine's parent folder (if it has
`Project.json`), or the engine itself. Place `--project` before the command.

## Commands

| Command | Options | Job |
| --- | --- | --- |
| `build` | `--release`, `--clean` | Builds the game (Debug; Release with `--release`). `--clean` removes the build folder first |
| `run` | `--release` | Builds and runs the game; returns when the game closes |
| `release` | | Builds Release and creates the `Release/<name>/` package folder and ZIP ([Release](Release.md)) |
| `test` | `--filter RE`, `--label RE`, `--list`, `--clean` | Builds and runs headless tests; report `<tests.buildDirectory>/reports/<run-id>/results.json` |
| `editor` | `--release`, `--build-only`, `--clean` | Builds the editor and project preview worker, opens the editor |
| `refresh` | | Build & Refresh in the project's open editor (joins an existing refresh); streams output and exits with its result. The preview stays open during the build. Returns 2 if the editor is not open ([control gateway](Editor/Architecture.md#control-gateway)) |
| `new` | `--name AD`, `--dry-run` | Sets up the engine's parent folder as a new project; asks for the name without `--name` |
| `info` | | Engine and project information, JSON (below) |
| `check` | `--fix` | Internals include boundary ([API boundaries](Contributing/ApiBoundaries.md)) and tuning file validity / canonical format ([Tuning](Tuning.md)); `--fix` formats valid tuning files |

Settings are in `Project.json` ([ProjectSetup](ProjectSetup.md)).

## `info` output

```json
{
  "engine": { "name": "...", "version": "0.1.0", "root": "C:\\...\\MyGame\\Engine" },
  "toolsContract": 1,
  "project": { "name": "MyGame", "root": "C:\\...\\MyGame" }
}
```

If the project root has no `Project.json`, `project` is `null`.

## Manifest

`EngineManifest.json` is the engine's identity and part of the contract: when populating the project list, the launcher
does not start Python for each project; it reads `Engine/EngineManifest.json` directly and checks `toolsContract`.

```json
{
  "name": "...", "version": "0.1.0", "toolsContract": 1,
  "repository": "https://.../engine.git",
  "notes": [ { "version": "0.1.0", "date": "2026-10-31", "changes": ["..."],
               "breaking": [ { "change": "...", "migrate": "..." } ] } ]
}
```

| Field | Meaning |
| --- | --- |
| `version` | `ANA.ARA.YAMA`. In 0.x, an ARA increase, and after 1.0, an ANA increase, is a breaking change: a project built with the previous version needs manual changes |
| `toolsContract` | version of this document |
| `repository` | engine repo address (submodule source in developer mode) |
| `notes` | cumulative patch notes, newest first; the first matches `version`. `breaking` lists breaking changes and how to comply (`migrate`); empty list if none. The launcher shows all intervening notes on update |

Rules are in `Tools/manifest.py` `note_problems`; `Tools/Tests/test_manifest.py` checks the real manifest, so a version
cannot increase without notes. The launcher's engine copy has a generated `EngineCopy.json` beside it (source tag /
commit, file hashes); engine tools do not read it.

## Environment variables

| Variable | Reader | Meaning |
| --- | --- | --- |
| `ENGINE_LAUNCHER` | editor | Full path to the windowed executable of the launcher that started the editor. If present, "Back to project list" appears in File: the editor starts that executable and closes. The launcher sets it for every engine command; tools forward it unchanged to the editor |

## Reserved name

`<engine> launcher ...` is for the launcher's own commands; the launcher forwards all other arguments unchanged to the
project's engine. The engine does not add a command named `launcher` (`test_cli.py` checks this).

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Project work failed: build error, failed test, release check, game / editor exited with an error code, `check` found a violation or tuning problem, `refresh` failed or was cancelled |
| 2 | Usage or infrastructure: wrong argument, missing tool (MSVC, CMake, Ninja), invalid `Project.json`, test report could not be created |
| 130 | Interrupted with Ctrl+C |

Error messages are written to stderr as `error: ...`.

## Changing the contract

- Adding a command, option, or field to `info` does not break the contract.
- Adding a manifest field does not break it.
- Removing or changing the meaning of commands / options, or changing exit codes, `info`, or manifest fields, breaks the
  contract: increment `EngineManifest.json` `toolsContract` and update this document in the same commit.
- Contract test: `Tools/Tests/test_cli.py` (command and option lists, `info`, exit codes).