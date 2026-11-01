# Editor automation

Scripts and agents drive the open editor through `python Engine ctl <command>` (from the project root; `yadien ctl` with
the launcher). Each command sends one request to the editor's [control endpoint](Architecture.md#control-gateway) and
prints its answer as one JSON object on stdout. Game Flows are editor scenes, so everything below applies to them too.
Design record: `Docs/Design/README.md` → D20 (engine repository).

For visual or gameplay requests, follow the [Visual work loop](../../AgentDocs/Index.md#visual-work-loop): create a
focused work scene, refresh, open it, read screenshots / frame grids, fix obvious defects and leave it open.

## Commands

| Command | Answer |
| --- | --- |
| `status` | `phase`, `status`, `ready`, `building`, `scene`; while ready also `paused`, `speed`, `seconds`, `canvas` |
| `list` | `scenes`: id, name, category, `parameters` (id, kind, apply, current `value`, range or `choices`), `actions` |
| `open ID` | Selects and starts the scene, waits until it runs (no-op if it already runs); status |
| `set ID=VALUE ...` | Sets parameters of the open scene; `parameters` as accepted. VALUE is JSON (`24`, `true`, `[1, 0, 0]`, `"#ff8800"`), otherwise text (choices). Before the scene runs, values are stored for its start |
| `action ID` | Runs a scene action; status |
| `pause`, `play` | Playback; status |
| `step [FRAMES]` | Pauses and advances FRAMES × 1/60 s (default 1, max 600); status |
| `speed X` | Playback speed, 0.1–4; status |
| `reset` | Restarts the scene with its parameters, waits until it runs; status |
| `seek SECONDS` | Pauses at that playback time (0–600); status. Going back replays from 0 with the current parameters and re-runs the actions taken since open / reset at their times (an action after a rewind replaces the later ones) |
| `screenshot [--window] [--out PATH] [--scale S] [--crop X Y W H]` | Writes the current frame as PNG; status + `path`, `width`, `height`. Crop is in canvas units, applied before scaling (0.05–4). `--window`: the whole editor window instead (panels included, no scene needed; crop in window pixels; also `window` size) |
| `frames --from A --to B [--count N] [--columns C] [--out PATH] [--scale S] [--crop X Y W H]` | One grid PNG of N frames between A and B seconds (pauses there); status + `path`, `frames` (times), `columns` ([Animation](#screenshots)) |
| `inspect` | Status + `inspection` (readout values, tables, focus, disabled controls) |
| `logs [--since N] [--limit N] [--level L]` | `entries` (sequence, level, source, text) after N, newest `limit` (100, max 1000) at or above L; `last` sequence |
| `refresh` (top-level command) | Build & Refresh; streams build output ([Tools contract](../ToolsContract.md)) |

Exit codes: 0 answered; 1 the editor refused the request (`error: ...` on stderr: unknown scene, invalid value, not
running, timed out); 2 no editor or wrong arguments.

## Guarantees

- **Fresh state:** `set`, `action`, playback commands, `seek`, `screenshot` and `inspect` answer only after the worker
  has applied every earlier request and drawn a frame after that. A screenshot never shows the state before a change.
- **Reproducible time:** playback `seconds` count from open / reset. `seek` and `step` advance in fixed 1/60 s steps, as
  fast as the worker can draw, so the same parameters, actions and time give the same frame for deterministic scenes.
  Pointer and keyboard input is not recorded.
- **Visible changes:** each state-changing command shows "Control: ..." over the viewport for a few seconds and is logged
  in Output. The developer's own input keeps working; the editor does not lock.
- **No blocking dialogs:** a Build & Refresh started through the endpoint (`refresh`) reports a failure to its client
  and as a notice; it does not open the failure dialog. A newer attempt closes a dialog left from an older one.
- **The right editor:** the editor's Console starts its shell with `ENGINE_EDITOR_CONTROL` (that editor's pipe), so
  commands from the Console reach it even when several editors are open. Elsewhere the build directory's
  `EngineEditor.control.json` announcement is used.

## Screenshots

Default location: `<editor build directory>/editor-captures/<kind>-<time>.png` (`shot`, `frames`, `mark`); only the
newest 50 are kept there (`--out` files are never pruned). A full 1600×900 canvas is large for an agent's context;
prefer `--scale 0.5` or `--crop` on the area of interest. Visual output is for looking, not for asserting in tests
([Testing](../Contributing/Testing.md)).

**Animation:** `ctl frames --from A --to B [--count N]` seeks to N evenly spaced times (2–16, default 9) and writes one
grid image, each tile labelled with its playback time; the answer lists the times. Without `--scale` the grid fits about
1600 px across, so a whole animation costs about one screenshot. `--crop` and `--columns` work as for screenshots.

## Marking: the developer points at something

The toolbar's Mark button covers the editor below the toolbar with a marking layer: drag a region, or click for a small
square (Esc or right-click cancels; one mark per press). Alt+drag on the scene marks without the button.

- **On the scene** (the region starts and ends on the image): the scene pauses so the frame holds still, the region
  of the viewport frame is saved, and the pasted line carries the scene's context:

  ```text
  [editor] scene=my.scene t=3.00s(paused) region=420,130 300x200 of 1600x900 image="C:\...\mark-....png" params: speed=2.0
  ```

  `region` is in canvas units; `params` lists values that differ from their initial ones. Reproduce the state with
  `ctl open`, `ctl set` and `ctl seek`.
- **Anywhere else** (a panel, the Inspector, or no scene running): the editor window is captured after the next frame
  (without the marking layer) and the line names the window region in framebuffer pixels:

  ```text
  [editor] window region=12,340 420x260 of 2560x1440 image="C:\...\mark-....png" open-scene=my.scene
  ```

The line is pasted into the Console's input without Enter; the developer types their question after it. The image is
the marked region only (scaled down to at most 1000 px on its long side).

## Build errors

A failed `refresh`, `build`, `test` or `editor` build prints `Errors (N):` after its output: one
`file:line:column: CODE message` line per distinct compiler, linker or CMake error (paths relative to the project).

## Limits

One request at a time: a second client waits up to 5 s while another is being answered (a `refresh` holds the endpoint
until its build ends). Commands that need a running scene fail while a profiler run owns it. Waiting for a scene to
start times out after 90 s; other waits after 15 s (`seek` adds time for long seeks).
