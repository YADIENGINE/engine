# Editor automation

Scripts and agents drive the open editor through `python Engine ctl <command>` (from the project root; `yadien ctl` with
the launcher). Each command sends one request to the editor's [control endpoint](Architecture.md#control-gateway) and
prints its answer as one JSON object on stdout. Game Flows are editor scenes, so everything below applies to them too.
Design record: `Docs/Design/README.md` → D20 (engine repository).

Agents follow the [editor skill](../../AgentDocs/Skills/editor/SKILL.md) to show something and the
[visual-change skill](../../AgentDocs/Skills/visual-change/SKILL.md) to change it: create a focused work scene, refresh,
open it, read screenshots / frame grids, fix obvious defects and leave it open.

## Commands

| Command | Answer |
| --- | --- |
| `status` | `editor` (`open`), `phase`, `status`, `ready`, `building`, `scene`, `control`; while ready also `paused`, `speed`, `seconds`, `canvas`; with no scene a `next` hint |
| `list` | `scenes`: id, name, category, `parameters` (id, kind, apply, current `value`, range or `choices`, `applyAction` when set), `actions` (`primary` when set). `--brief`: only id, name, category |
| `find WORD... [--limit N]` | `matches` (best N, default 5): id, name, category, `matched`, `set` (choices the words select), `action`, `commands` that open it, compact `parameters`, `actions`; `more` when cut ([Finding a scene](#finding-a-scene)) |
| `describe ID` | `scene`: that scene's `list` entry; exit 1 with close ids when unknown |
| `open ID [--set ID=VALUE...] [--action ID] [--screenshot [--scale S]]` | Selects and starts the scene, waits until it runs (no-op if it already runs); status. The options run `set` (and `reset` when an on-reset value changed), `action` and `screenshot` in that order and merge their answers |
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
| `note [WORD...] [--state working\|waiting\|idle] [--hook]` | Reports what the agent is doing (default `working`); `--hook`: see Agent hooks below. Run from a Console session (`ENGINE_EDITOR_SESSION` set, sent as `session`), it sets that session's state and hover text until the developer types into it again. `state`, `text` (up to 200 bytes), `control` |
| `audio` | The Audio Preview panel: `loaded`, `catalog` (whether a worker has reported the audio catalog), `loop`, `volume`; with a sound also `file`, `asset` (path in the asset folder), `playing`, `paused`, `position`, `length` (seconds), `waveform` (`decoding` / `ready` / `failed`), `uses` (catalog entries: kind, id, name, label), `error` |
| `audio play FILE [--at S] [--loop\|--no-loop]` | Loads the sound into Audio Preview (shown if hidden) and plays it from its source file; the scene is not touched. FILE is absolute or relative to the current folder, project root or asset folder. Answers the `audio` state once playing; exit 1 when it cannot be loaded |
| `audio pause`, `resume`, `stop`, `seek S`, `volume V`, `loop on\|off` | The loaded sound's transport; `seek` starts a stopped sound there, `volume` is 0–1; the `audio` state |
| `refresh` (top-level command) | Build & Refresh; streams build output ([Tools contract](../ToolsContract.md)) |

Exit codes: 0 answered; 1 the editor refused the request (`error: ...` on stderr: unknown scene, invalid value, not
running, timed out, agent control off); 2 no editor or wrong arguments.

**Agent hooks:** an agent CLI's own events report a session's state more reliably than the editor's guess (input,
output, terminal bell). `ctl note --hook` is made for them: it prints nothing, always exits 0 (Claude Code blocks a
prompt or a stop on exit 2) and does nothing outside an editor Console session. Claude Code, in the project's
`.claude/settings.json` (Git Bash runs hook commands when installed; `CLAUDE_PROJECT_DIR` is the project root):

```json
{
  "hooks": {
    "UserPromptSubmit": [{"hooks": [{"type": "command", "shell": "bash", "timeout": 10,
      "command": "python \"$CLAUDE_PROJECT_DIR/Engine\" ctl note --hook --state working"}]}],
    "PostToolUse": [{"hooks": [{"type": "command", "shell": "bash", "timeout": 10, "async": true,
      "command": "python \"$CLAUDE_PROJECT_DIR/Engine\" ctl note --hook --state working"}]}],
    "Notification": [{"matcher": "permission_prompt", "hooks": [{"type": "command", "shell": "bash", "timeout": 10,
      "command": "python \"$CLAUDE_PROJECT_DIR/Engine\" ctl note --hook --state | `note [WORD...] [--state working|waiting|idle] [--hook]` |"}]}],
    "Stop": [{"hooks": [{"type": "command", "shell": "bash", "timeout": 10,
      "command": "python \"$CLAUDE_PROJECT_DIR/Engine\" ctl note --hook --state idle"}]}]
  }
}
```

`permission_prompt` keeps Claude Code's idle reminder from showing as a question; PostToolUse returns the session to
working after an approval. An interrupted turn (Esc) runs no Stop hook; the state follows your next input. Codex runs
`notify` (in `~/.codex/config.toml`) only when a turn completes:
`notify = ["python", "<project>/Engine", "ctl", "note", "--hook", "--state", "idle"]` (the JSON payload it appends is
ignored).

**Agent control:** the developer can turn off Agent → Agent control ([Agent workspace](Usage.md#agent-workspace)).
While it is off, `status` reports `"control": false` and commands that change the editor (`open`, `set`, `action`,
`pause`, `play`, `step`, `speed`, `reset`, `seek`, `frames`, `refresh`, and `audio` with an action) are refused;
`status`, `list`, `find`, `describe`, `screenshot`, `inspect`, `logs`, `note` and plain `audio` still answer.

## Finding a scene

A request names a subject ("the ice dragon finale"), not a scene id. `ctl find ice dragon finale` searches every
scene's name, id, category, description, parameters, actions and **parameter choices**, so a subject that is a choice
of a shared scene (one enemy of a fight scene) is found without project-specific notes. Matches are ranked by how many
words they contain, then by where: name, id, choice, parameter or action, category, description.

For each match, words not already in the scene's name or id select the choice they match best (`set`). `commands` holds
one `open` call that shows it: `--set` for those choices, `--action` with the action that applies next-action values
(the parameter's `applyAction`, else the scene's only primary action, else its only action; otherwise a `note`) and
`--screenshot --scale 0.5`.

```text
$ python Engine ctl find ice dragon finale
{"query": "ice dragon finale", "matches": [{"id": "myproject.fight", "set": {"enemy": "Ice Dragon", "stage": "Finale"},
 "action": "start", "commands": ["python Engine ctl open myproject.fight --set \"enemy=Ice Dragon\"
 stage=Finale --action start --screenshot --scale 0.5"], "parameters": {"enemy": "Slime|Fire Dragon|Ice Dragon; now Slime",
 ...}, ...}]}
```

Use `describe ID` when the compact `parameters` are not enough; `list` is the whole catalog and can be large.

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
