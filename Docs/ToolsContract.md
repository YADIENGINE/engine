# Tools contract

Current contract: **2** (engine 0.3.0). Contract 1 used `preview` / `previews` in `ctl` JSON;
contract 2 uses `scene` / `scenes`. Command names and exit codes are unchanged. The editor control endpoint
uses protocol version 3, so rebuild the editor with its tools. See the
[cumulative migration table](Editor/Development/Migrations.md).

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

`crash REPORT [--symbols DIR] [--json]` inspects a report directory or `report.json` without modifying it; missing
symbols return a readable partial report (0), invalid input or unavailable resolution tools return 2.

Local crash artifacts: [Debugging](Debugging.md). `run`, `sim`, `refresh` and `test` expose only their own process
failures. `build` compiles without starting the game or scanning historical reports.

| Command | Options | Job |
| --- | --- | --- |
| `build` | `--release`, `--clean` | Builds the game (Debug; Release with `--release`). `--clean` removes the build folder first |
| `run` | `--release` | Builds and runs the game; returns when the game closes |
| `release` | | Builds Release and creates the `Release/<name>/` package folder and ZIP, without the sounds the game does not use ([Release](Release.md#only-the-sounds-a-game-uses)) |
| `test` | `--filter RE`, `--label RE`, `--list`, `--clean` | Builds and runs headless tests; report `<tests.buildDirectory>/reports/<run-id>/results.json` |
| `editor` | `--release`, `--build-only`, `--clean` | Builds the editor and project worker process, opens the editor |
| `refresh` | | Build & Refresh in the project's open editor (joins an existing refresh); streams output and exits with its result. The open scene stays open during the build. Returns 2 if the editor is not open ([control gateway](Editor/Architecture.md#control-gateway)) |
| `ctl` | `status`, `list`, `open ID`, `set ID=VALUE...`, `action ID`, `pause`, `play`, `step [N] [--sim]`, `speed X`, `reset`, `seek S`, `seek --step N`, `replay load\|save FILE`, `screenshot [--window] [--out P] [--scale S] [--crop X Y W H]`, `frames --from A --to B [--count N] [--columns C] [--steps]`, `inspect`, `logs [--since N] [--limit N] [--level L]`, `audio [play F [--at S] [--loop] \| pause \| resume \| stop \| seek S \| volume V \| loop on\|off]` | Drives and observes the project's open editor; prints one JSON answer. Returns 1 when the editor refuses the request, 2 if no editor is open ([Editor automation](Editor/Automation.md)) |
| `asset` | `new NAME [--folder F]`, `mkdir F`, `move SRC DEST`, `unlisted [FILES...]` | The project's graphic assets under `Game/Assets/Graphics`: a graphic's skeleton, a folder, or a move that updates includes; JSON result. `unlisted` lists free `void DrawX(Painter& ...)` header functions whose names occur in no project `.asset.cpp`: JSON array of `file`, `line`, `function`; defaults to `Game/`, skips `build/`, `Engine/`, `Launcher/`, and always returns 0 (reminder only). Missing/deleted or non-header inputs are ignored ([Graphic assets](Editor/Api.md#graphic-assets)) |
| `audio` | `info FILE... [--silence-db DB]`, `waveform FILE [--out P] [--width W] [--height H]`, `usage [FOLDER] [--unused \| --missing]`, `catalog` | Facts about sound files and the project's audio catalog, without an open editor: the built editor worker decodes. JSON answer, with a `note` when the engine's starter worker answered and the game's sound tables are unknown; returns 1 when a file cannot be decoded, 2 when no editor worker is built ([Audio tools](AudioTools.md)) |
| `sim` | `list`, `run SCENARIO [--out TAPE]`, `verify TAPE`, `diverge TAPE`, `state TAPE [--step N]`, each with `--release` | Builds the project's simulation tool and runs it without a window; one JSON answer on stdout, build output on stderr. Returns 1 when an expectation or a hash check fails, 2 for a bad request or a project with no simulation ([Simulation](Simulation.md#tools)) |
| `new` | `--name AD`, `--dry-run` | Sets up the engine's parent folder as a new project; asks for the name without `--name`. A name is a letter followed by letters, digits or `_`, at most 64 characters (ASCII, no spaces); any other name returns 2, so a launcher can check it while the user types |
| `prebuilt` | `--output FOLDER` | Compiles this engine into a package that projects link, replacing `FOLDER` ([Prebuilt engine](Prebuilt.md)) |
| `info` | | Engine and project information, JSON (below) |
| `api` | `QUERY...` | Finds engine API without an editor or a build: a name (`StepsIn`, `CollisionWorld::Sweep`, also `Game::Sounds()`) prints its declarations as `path:line signature  // comment` (a type with its line range and members; an enum value with its line; a protected or private member in one line saying so; for a known type without that member, its closest members); words print the best routes of `AgentDocs/Tasks.json` (files, recipe, concept docs) and the declarations sharing the most words. Paths open from the project root; at most 40 lines. Read live from the public headers. Returns 1 when nothing matches ([Repository layout](Contributing/RepositoryLayout.md#documentation-ownership)) |
| `check` | `--fix` | Internals include boundary ([API boundaries](Contributing/ApiBoundaries.md)), tuning validity / canonical format ([Tuning](Tuning.md)), and engine AgentDocs: recipe snippets, skills, links, task routes and API names; `--fix` formats valid tuning files and resyncs recipe snippets |
| `agent-report` | `--harness all\|claude\|codex`, `--since DATE`, `--until DATE`, `--session ID`, `--no-subagents`, `--top N`, `--json`, `--claude-home DIR`, `--codex-home DIR` | Tokens coding agents spent on the project, read from the session logs Claude Code and Codex write; prints text or JSON, writes nothing. Returns 2 when no session started in the project ([Agent metrics](Contributing/AgentMetrics.md#agent-report)) |
| `agent-eval` | `list`, `verify`, `run [--harness codex\|claude] [--effort m\|h] [--repeat N]`, `watch [RUN]`, `show [RUN]`, `compare [RUN] [--against RUN]`, `baseline [RUN]`; `verify` and `run` take `--task ID`, `--all`, `--smoke`, `--keep` | Engine contributors: fixed agent tasks on a fixture project, graded by machine and measured in tokens; results under `build/agent-eval/` ([Agent metrics](Contributing/AgentMetrics.md#agent-eval)) |

Failed builds (`build`, `run`, `release`, `test`, `editor`, `refresh`) end their output with an `Errors (N):` summary of
the distinct compiler, linker and CMake errors ([Editor automation](Editor/Automation.md#build-errors)).

`build`, `run`, `release`, `test`, `sim` and `editor` build one at a time per build folder: a second command building
the same folder (two `sim run` started together) waits for the first and says so on stderr, instead of failing on a
file the compiler holds. The lock is in the system temp folder (`engine-build-locks/`) and ends with its process.

Under a coding agent, `build`, `run`, `release`, `test`, `sim`, `editor` and `refresh` print only their result: a success
line, or the `Errors (N):` summary (the end of the output when no error is recognized) and the path of the full log.
Build output always goes to `build.log` in the build folder (`refresh.log` in the editor's for `refresh`); `test` keeps
its stage logs in the report folder and shows at most 30 lines of a failed test's diagnostics. Agents read everything a
command prints again on every later request, so the full output stays in the file.

Settings are in `Project.json` ([ProjectSetup](ProjectSetup.md)).

## `info` output

```json
{
  "engine": { "name": "...", "version": "0.3.0", "root": "C:\\...\\MyGame\\Engine" },
  "toolsContract": 2,
  "project": { "name": "MyGame", "root": "C:\\...\\MyGame" }
}
```

If the project root has no `Project.json`, `project` is `null`.

## Manifest

`EngineManifest.json` is the engine's identity and part of the contract: when populating the project list, the launcher
does not start Python for each project; it reads `Engine/EngineManifest.json` directly and checks `toolsContract`.

```json
{
  "name": "...", "version": "0.3.0", "toolsContract": 2,
  "repository": "https://.../engine.git"
}
```

Each version's notes are one file, `ReleaseNotes/<version>.json`; a new version adds its file and leaves the others alone:

```json
{ "name": "Cats", "summary": "One sentence.", "date": "2026-11-01", "changes": ["..."],
  "breaking": [ { "change": "...", "migrate": "..." } ] }
```

`name` is the version's name, shown as `0.11.1 Cats`; `summary` is one sentence the launcher shows when the version is
new. Both are required in the newest note; versions before 0.11.1 have none and the launcher calls them Pre-Release.
Each GitHub release carries `ReleaseNotes.json`, every note of its tag in one list (newest first, each with its
`version`), so the launcher shows the notes of versions it has not downloaded.

| Field | Meaning |
| --- | --- |
| `version` | `ANA.ARA.YAMA`. In 0.x, an ARA increase, and after 1.0, an ANA increase, is a breaking change: a project built with the previous version needs manual changes |
| `toolsContract` | version of this document |
| `repository` | engine repo address (submodule source in developer mode) |

In a note, `breaking` lists breaking changes and how to comply (`migrate`); empty list if none. The launcher shows the
notes of every version between a project's engine and the new one, so old files stay. Engines before 0.4.0 keep their
notes in the manifest's `notes` field, and the launcher still reads those tags.

Rules are in `Tools/manifest.py` `note_problems`; `Tools/Tests/test_manifest.py` checks the real files, so a version
cannot increase without notes. The launcher's engine copy has a generated `EngineCopy.json` beside it (source tag /
commit, file hashes); tools read only its `files`, to tell whether a prebuilt engine still matches the copy.

## Environment variables

Diagnostics also uses `ENGINE_CRASH_DIRECTORY` (a launch-owned artifact root, suppressing player notification),
`ENGINE_CRASH_SESSION` (the editor's unique worker-session id), and `ENGINE_CRASH_REPORTER` (an explicit helper path
for diagnostic probes). Tools set these for their children; they are not project settings. An inherited crash
directory must not be reused to infer which command failed.

| Variable | Reader | Meaning |
| --- | --- | --- |
| `ENGINE_LAUNCHER` | editor | Full path to the windowed executable of the launcher that started the editor. If present, "Back to project list" appears in File: the editor starts that executable and closes. The launcher sets it for every engine command; tools forward it unchanged to the editor |
| `ENGINE_PREBUILT` | `build`, `run`, `release`, `editor` | Folder of the compiled engine for the project's version ([Prebuilt engine](Prebuilt.md)). The launcher sets it when it has one; tools remember it for commands run without the launcher |
| `ENGINE_ASSET_REPORT` | a built game | When set, the game prints its asset list as `asset: <path>` lines and exits before opening a window. `release` sets it |
| `ENGINE_ASSET_DIRECTORY` | a built game | The `Assets` folder to read instead of the one beside the executable. The editor sets it for its worker, which runs from a snapshot copy of the executable |
| `CLAUDECODE`, `CODEX_CI` | `build`, `run`, `release`, `test`, `sim`, `editor`, `refresh` | Set by Claude Code and Codex: the command runs under a coding agent and prints only its result (see [Commands](#commands)) |
| `ENGINE_AGENT` | same | `1` gives the agent output anywhere, `0` turns it off under an agent |
| `ENGINE_EDITOR_CONTROL` | `refresh`, `ctl` | The control pipe of the editor whose Console started the command; used instead of the build directory's announcement. The editor sets it for its Console's shell |

## Project status files

Optional files in a project's build directories, for a launcher to show the project. Older engines do not write them, so
a reader treats a missing or unreadable file as "nothing known"; deleting them is always safe.

| File | Writer | Contents |
| --- | --- | --- |
| `<game.buildDirectory>/LastBuild.json` | `build`, `run` | `{"ok": bool, "finished": "2026-11-02T12:30:05Z"}` (UTC) |
| `<editor.buildDirectory>/LastBuild.json` | editor | The same, for its last Build & Refresh |
| `<editor.buildDirectory>/LastFrame.png` | editor | A recent frame of the running scene, at most 960 px on its longer side |

## Reserved name

`<engine> launcher ...` is for the launcher's own commands; the launcher forwards all other arguments unchanged to the
project's engine. The engine does not add a command named `launcher` (`test_cli.py` checks this).

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Project work failed: build error, failed test, release check, game / editor exited with an error code, `check` found a boundary, tuning or AgentDocs problem, `refresh` failed or was cancelled |
| 2 | Usage or infrastructure: wrong argument, missing tool (MSVC, CMake, Ninja), invalid `Project.json`, test report could not be created |
| 130 | Interrupted with Ctrl+C |

Error messages are written to stderr as `error: ...`.

## Changing the contract

- Adding a command, option, or field to `info` does not break the contract.
- Adding a manifest field does not break it.
- Removing or changing the meaning of commands / options, or changing exit codes, `info`, or manifest fields, breaks the
  contract: increment `EngineManifest.json` `toolsContract` and update this document in the same commit. Such an engine
  is released together with a launcher that lists the new contract in `LauncherManifest.json` `toolsContracts`.
- Contract test: `Tools/Tests/test_cli.py` (command and option lists, `info`, exit codes).
