# Editor automation

Scripts and agents drive the open editor through `python Engine ctl <command>` (from the project root; `yadien ctl` with
the launcher). Each command sends one request to the editor's [control endpoint](Architecture.md#control-gateway) and
prints its answer as one JSON object on stdout. Game Flows are previews, so everything below applies to them too.
Design record: `Docs/Design/README.md` → D20 (engine repository).

Typical loop: change code → `refresh` → `ctl open` → `ctl set` / `ctl seek` → `ctl screenshot` → read the PNG → fix →
repeat; leave the editor showing the result for the developer.

## Commands

| Command | Answer |
| --- | --- |
| `status` | `phase`, `status`, `ready`, `building`, `preview`; while ready also `paused`, `speed`, `seconds`, `canvas` |
| `list` | `previews`: id, name, category, `parameters` (id, kind, apply, current `value`, range or `choices`), `actions` |
| `open ID` | Selects and starts the preview, waits until it runs (no-op if it already runs); status |
| `set ID=VALUE ...` | Sets parameters of the open preview; `parameters` as accepted. VALUE is JSON (`24`, `true`, `[1, 0, 0]`, `"#ff8800"`), otherwise text (choices). Before the preview runs, values are stored for its start |
| `action ID` | Runs a preview action; status |
| `pause`, `play` | Playback; status |
| `step [FRAMES]` | Pauses and advances FRAMES × 1/60 s (default 1, max 600); status |
| `speed X` | Playback speed, 0.1–4; status |
| `reset` | Restarts the preview with its parameters, waits until it runs; status |
| `seek SECONDS` | Pauses at that playback time (0–600); status. Going back replays from 0 with the current parameters and re-runs the actions taken since open / reset at their times (an action after a rewind replaces the later ones) |
| `screenshot [--out PATH] [--scale S] [--crop X Y W H]` | Writes the current frame as PNG; status + `path`, `width`, `height`. Crop is in canvas units, applied before scaling (0.05–4) |
| `frames --from A --to B [--count N] [--columns C] [--out PATH] [--scale S] [--crop X Y W H]` | One grid PNG of N frames between A and B seconds (pauses there); status + `path`, `frames` (times), `columns` ([Animation](#screenshots)) |
| `inspect` | Status + `inspection` (readout values, tables, focus, disabled controls) |
| `logs [--since N] [--limit N] [--level L]` | `entries` (sequence, level, source, text) after N, newest `limit` (100, max 1000) at or above L; `last` sequence |
| `refresh` (top-level command) | Build & Refresh; streams build output ([Tools contract](../ToolsContract.md)) |

Exit codes: 0 answered; 1 the editor refused the request (`error: ...` on stderr: unknown preview, invalid value, not
running, timed out); 2 no editor or wrong arguments.

## Guarantees

- **Fresh state:** `set`, `action`, playback commands, `seek`, `screenshot` and `inspect` answer only after the worker
  has applied every earlier request and drawn a frame after that. A screenshot never shows the state before a change.
- **Reproducible time:** playback `seconds` count from open / reset. `seek` and `step` advance in fixed 1/60 s steps, as
  fast as the worker can draw, so the same parameters, actions and time give the same frame for deterministic previews.
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

The toolbar's Mark button (or Alt+drag on the preview) lets the developer drag a region, or click for a 160-unit square.
The preview pauses so the frame holds still, the region is saved as `mark-<time>.png`, and one line is pasted into the
Console's input without Enter, e.g.:

```text
[editor] preview=my.preview t=3.00s(paused) region=420,130 300x200 of 1600x900 image="C:\...\mark-....png" params: speed=2.0
```

The developer types their question after it. The image is the marked region only; `params` lists values that differ
from their initial ones. An agent reproduces the state with `ctl open`, `ctl set` and `ctl seek` and reads the image.
While the Mark toggle is on (one mark per press), presses on the preview do not reach the product.

## Build errors

A failed `refresh`, `build`, `test` or `editor` build prints `Errors (N):` after its output: one
`file:line:column: CODE message` line per distinct compiler, linker or CMake error (paths relative to the project).

## Limits

One request at a time: a second client waits up to 5 s while another is being answered (a `refresh` holds the endpoint
until its build ends). Commands that need a running preview fail while a profiler run owns it. Waiting for a preview to
start times out after 90 s; other waits after 15 s (`seek` adds time for long seeks).
