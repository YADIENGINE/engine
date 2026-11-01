# Editor usage

## Window

Saved dock layout and panel visibility are retained; Viewport starts open and in front on every editor launch. On first
launch, Develop shows Project, Viewport, Inspector, and Tuning (a tab in the same right area as Inspector); Output,
Profiler, and Console are closed. The bottom status bar has Output and Console buttons.

- **Left (Project):** search, category, All / Favorites / Recent. Single-click opens content; Ctrl+P focuses search. The
  star toggles favorites. Recent keeps the last 10 successful opens.
- **Center (Viewport):** the worker's actual image. In the panel's local View menu, Fit shows the whole canvas, Focus
  content shows the area reported by content, and 100% shows logical size. Zoom in the same menu or Ctrl+wheel zooms;
  middle-button dragging pans; Reset pan and zoom resets view settings. Without Ctrl, the wheel goes to the product. The
  maximize button or double-clicking the view summary enables temporary focus mode. Closed, loading, or failed editor scenes
  show a state explanation and an available start / retry / cancel action. Playback is only in the global toolbar.
- **Right (Inspector):** the header's `?` opens the description by tooltip or click. Property search and the Modified
  counter find changed fields; with at most six visible fields, the Search button opens search. An accent dot marks a
  value different from default; Pending marks a value awaiting worker response, not a setting applied on the next
  action. Reset icons appear on changed fields, or on hover / keyboard focus for others, resetting only that field. The
  primary action is fixed at the panel bottom; settings, secondary Actions, and Live information scroll in their own
  area. Choice controls are searchable; large integers such as seeds can be typed directly. Choice buttons use full
  panel width immediately below the label. Normal controls move below labels in narrow panels; checkboxes stay on the
  label line when they fit. Normal fields share a label column; button and field names truncate instead of wrapping
  letter by letter, with full text in tooltips. Advanced groups start collapsed; search and Modified filters expand
  matching groups. Live information shows numbers right-aligned in technical font, short statuses as badges, and
  explanations wrapped.
- **Output:** `Last Build Try` shows output from the latest Build & Refresh click until the scene is ready, fails, or
  is cancelled; `Full` shows the last 1000 lines from editor, worker, and all attempts. The latest attempt's start time,
  duration, result, and build exit code are visible. Info / Warning / Error counters, search, and Copy Visible apply to
  the selected scope. Click selects one line, Ctrl+click multiple lines, Shift+click a range. With the line area
  focused, Ctrl+A selects visible lines, Ctrl+C copies selection; right-click also offers Copy Selected / Copy Visible.
  Double-click opens the message in read-only details with mouse text selection. Normal Ctrl+A / Ctrl+C work in search
  and detail text fields. Follow stops on scrolling up or selecting a line. Clear clears only the Full buffer; the last
  attempt and files remain. `Open Log` opens the selected scope's full file. Files are kept per session and attempt
  under `editor-logs/` in the build directory; they are not deleted automatically. The last-attempt buffer is limited to
  10000 lines / 8 MiB; on overflow, the first error is retained and a reference to the full file appears. Build failures
  or refresh failures after a successful build open a popup once; cancellation does not. Read-only popup text supports
  mouse selection and Ctrl+C; Ctrl+A selects all text. Copy copies the error summary, build exit code if present, and
  first error message together. Show Output exits maximized scene, brings Output forward, opens Last Build Try, and
  scrolls to the first error. In this view, previous search/level preferences are temporarily removed; switching scope
  restores saved preferences.
- **Assets:** the asset library: back and search across the top, a folder tree on the left (sets are rows too), a tile
  grid on the right, and tile size at the bottom (or Ctrl+wheel). A click on a graphic opens it in the Viewport; a click
  on a folder or set enters it. Back (Alt+Left) undoes the last move or opened graphic, in order. A click on a sound
  loads it into Audio Preview and plays it; a second click on the playing sound stops it. Once a worker has reported
  the project's audio catalog, a sound the catalog does not list is dimmed, and the tooltip names the catalog entries
  that play it. Right-click for Copy usage, Copy path, Open code and
  Show in Explorer. The + button or a right-click creates a folder or graphic; dragging onto a folder moves it, updating
  includes. The Engine root is read-only. A graphic not built yet is dimmed; Build & Refresh also renews thumbnails.
  Registering assets: [Api](Api.md#graphic-assets).
- **Audio Preview:** the sound picked in Assets, beside it in the bottom dock by default. It shows the file name,
  folder, length, format, size, sample rate and channels; whether the audio catalog uses it, as `Effect <name>` or
  `Music <name>`, or `#<id>` when the catalog entry has no `name`; and a waveform (a plain bar until it has been decoded
  in the background). Click or drag on the waveform to seek. Below it: previous sound in the folder, play/pause (Space),
  stop, next sound, loop and volume. It plays in the editor process from the source file, so a new sound needs no Build
  & Refresh, and it leaves the viewport and running scene alone.
- **Console:** a real PowerShell session at the project root. Drag to select visible text and copy with Ctrl+C /
  Ctrl+Shift+C or right-click Copy. Without selection, Ctrl+C interrupts the running program; Ctrl+V pastes, Escape
  clears selection. Scrollback and double-click word selection are not yet available.
- **Tuning:** the project's `*.tuning.json` files ([Tuning](../Tuning.md)); the list comes from the build's
  `Tuning/TuningFiles.json`, with new files appearing after the next configure. File selection is at the top, followed
  by the scene's latest state for that file: Live, Not applied (invalid file; line and reason), Build needed (field /
  length changed; Build & Refresh button), Unreadable. If the scene does not use the file or is not open, this is
  stated. Value fields are label / box rows; tables are grids. Type numbers, vec2, range, and colors (`#RRGGBB[AA]`) and
  commit with Enter; leaving a box without Enter reverts the edit; checkboxes write on click. Out-of-bounds or invalid
  values are not sent; the box stays red, with the reason in a tooltip. An accepted value is patched as a single value
  into the current file on disk and written atomically (external changes are not overwritten); if the file is currently
  invalid, no write occurs and Output gets a warning. External edits (by hand or AI) refresh the panel. Add rows /
  fields in the file, not in the panel.
- **Profiler:** Live / Benchmarks / Compare. Opens when a benchmark example is selected; warmup / measurement frames /
  repetition pairs are under Advanced benchmark settings. Glow override is one choice group; highlighting shows the last
  requested value. Compare's Export menu offers JSON and Markdown. Details: [Profiler](Profiler.md).
- Panels can be docked, combined into tabs, closed, and reopened from Window. Scene → Language changes the selected
  scene's language; it can be chosen before opening a scene and is used in the opening recipe. Language control is
  not shown in Inspector. The menu always opens and explains scenes without language support. The top View menu
  manages Focus viewport / Restore workspace, UI scale (75% / 100% / 125% / 150%), and viewport performance metrics. The
  default 100% matches the previous 125% size; UI and technical text fonts scale with user scale and monitor DPI.

Under Window → Layouts, Develop opens general scene layout, Inspect a wider Inspector, and Profile a Profiler-focused
layout. Last custom layout restores the last manual layout; selecting a preset does not erase it. Bottom-bar Output /
Console buttons only toggle their respective panel; multiple panels can stay open. Window → Toggle last bottom tool or
Ctrl+J toggles the last-used bottom panel without affecting others. Profiler opens from Window or the command palette.
Unread errors or warnings flash the Output button; viewing Output clears the notification. Highlighting uses the theme's
error or warning color; errors take priority.

## Toolbar

The global toolbar has one primary action: Open, Stop, or Cancel according to state. Pause / Resume is a separate time
control. The primary action and Build & Refresh have labels when space allows; in narrow areas, secondary controls move
to an overflow menu. Tooltips show name, shortcut, and the reason if unavailable.

| Button | Meaning |
| --- | --- |
| Open / Stop / Cancel | Opens selected content when closed; closes the scene when ready or paused; cancels catalog reading, startup, building without a scene, or benchmarking. Cancel an ongoing build with an open scene through Cancel build in the command palette |
| Pause / Resume | Freezes Update and presentation time; drawing, parameters, actions, and Reset remain available |
| Step | Advances 1/60 s while paused |
| Speed | 0.1×..4× |
| Reset | Creates a new instance from the accepted recipe (clock starts at 0) |
| Build & Refresh | Builds the worker target; the scene keeps running throughout. A successful build takes selection, values, pause, and speed and replaces the old one when ready; errors leave the scene unchanged. From terminal: `python Engine refresh` |

Reset recreates the scene in the same worker process. Use Open to restart the worker process.

| Shortcut | Action |
| --- | --- |
| F5 | State-dependent primary action |
| F6 | Pause / Resume |
| F7 | Single frame while paused |
| Ctrl+R | Reset |
| Ctrl+Shift+B | Build & Refresh |
| Ctrl+P | Project search |
| Ctrl+Shift+P | Command palette |
| Ctrl+Shift+A | Open the chosen agent session (also while Console is focused) |
| Ctrl+J | Toggle last bottom tool panel |

Search scene actions, panel visibility, and layouts in the command palette; select with up / down, run with Enter,
close with Escape. Unavailable actions show their reason. Playback / build shortcuts do not run during text editing or
popups; input consumed by the editor is not forwarded to the scene.

Replay, Reset, and the product's own Pause screen differ: Replay restarts content's own presentation, Reset recreates
the instance, toolbar Pause stops time.

## When controls apply

- **Immediate:** applies immediately to the current appearance.
- **Next action:** prepares the next Replay / Emit / Apply offer.
- **Reset to apply:** used when creating a new instance.
- Groups with the same application time show one explanation; if content reports an action link, the actual button name
  such as Start / Replay is used. In mixed groups, the tooltip on `*`-marked fields explains application time.
- Controls changed by Randomize and content interactions are reflected back in the right panel. Random results are
  written to the recipe (seed); Reset recreates the same result.

## Product input

Navigation, hover, selection, and scroll input in the displayed product UI are preserved. Clicking the image gives
keyboard focus; Inspector text boxes and popups consume input. Dragging started in the image preserves release outside
the panel. Focus loss clears held keys.

## Real game flows

If the catalog offers such an entry, the worker can run production scenes through a custom Game adapter. Startup options
and in-memory profile policy are project-defined. The product's Back action belongs to game navigation; toolbar Stop
closes the worker. The host keeps OS window and capture size fixed.

## Agent workspace

The editor runs the developer's own agent CLI (Claude Code, Codex or any command) in a Console session and hands it
editor context; it never calls a model itself. The agent drives the editor back through `python Engine ctl`
([Automation](Automation.md)), and the project opts in to the engine's agent rules through `Engine/AgentDocs/Agent.md`
([Using an AI agent](../ProjectSetup.md#using-an-ai-agent)).

- **Console sessions:** the Console holds any number of sessions: shells and agent CLIs, named *Shell*, *Claude*,
  *Codex* (*Claude 2* for a second one). A narrow list on the right of the terminal shows each session with a terminal
  or robot icon; drag its left edge to resize it. Click a row to select it; the selected row is highlighted. Hover
  shows its title, state and current activity. A close button appears on hover; right-click also offers *Close*.
  The **+** button at the top (*Add terminal* on hover) offers Shell, Claude, Codex and *Custom agent*, with the named
  CLIs' commands alongside them. Choosing an agent makes it the one agent requests start next. Each session keeps
  running while the panel is hidden; closing one ends its program (a running agent asks first). Agent requests
  (Ctrl+Shift+A, Send scene, Marks, ...) go to the selected session when it runs an agent, else to the agent session
  selected last; with none, the chosen agent starts. The first draw opens a shell when there is none.
- **Session state** (each session's icon): *working* while its program writes output for a request, *waiting for you*
  when it rang the terminal bell after your last input (agent CLIs ring it when they ask something), *done* when the
  output went quiet after a request, *idle* before anyone typed into it, *exited* when the program ended. An agent's
  own `python Engine ctl note TEXT --state working|waiting|idle` from that session overrides the guess until you type
  again; its text, else the terminal title the CLI sets, is what the session shows on hover. Each session's program
  gets `ENGINE_EDITOR_SESSION`, which `ctl note` sends along. Working shows a clock; waiting shows a pulsing question
  mark in the warning color; done shows a green check. Idle has no state icon, and exited rows are dimmed.
- **Opening an agent:** Ctrl+Shift+A opens the chosen CLI in the project folder, or selects its session, including
  from Console. The session's shell stays open after the CLI exits. On first use, a setup modal lets you choose
  Claude Code, Codex or Custom, checks the executable on PATH (or at its full path), and offers an *Install...* link
  for the named CLIs. Custom shows a command field with an argument hint. *Check again* rechecks installation; if PATH
  changed, restart the editor or use a full path. *Start* accepts the choice and opens the session; *Cancel* leaves
  preferences unchanged. Setup also opens if the command is missing, with a message in Output. Context sent during
  setup waits for Start; Cancel discards that pending context. *Copy hint* copies: "Let the agent know the engine:
  add @Engine/AgentDocs/Agent.md to your CLAUDE.md (or AGENTS.md)".
- **Agent menu** groups Send context and Settings. Open agent and New shell are available through the Console's add
  button and remain in the command palette under "Agent:", alongside the context commands:
  - *Send scene* pastes the open scene's id, playback time, changed parameters and a capture of the frame
    (`editor-captures/scene-*.png`) into the agent session, without Enter: type your request after it.
  - *Send build failure* pastes the last failed Build & Refresh with its error lines; *Ask agent to fix build* also
    asks for a fix and presses Enter. The build failure popup makes *Ask Agent to Fix* the primary button, beside
    Copy, Show Output and Close. Disabled context commands explain why in a tooltip.
  - *Agent control* (on by default): off, tools may observe the editor (status, list, screenshot, inspect, logs) but
    every command that changes it (open, set, action, playback, seek, frames, refresh) is refused. The status bar shows
    "Agent control off".
  - *Agent CLI* uses radio choices for Claude Code (`claude`), Codex (`codex`) or Custom; the command field is shown
    only for Custom. *Agent control* is a checkbox with a tooltip explaining read-only access.
- **Output:** right-click → *Send Selected to Agent* pastes the selected lines.
- **Inspector:** right-click a parameter's label or value → *Send to Agent* pastes its current value, range or choices
  and description, without Enter. This is also available for disabled parameters.
- **Marks** (toolbar Mark, Alt+drag) go to the agent session when there is one.
- **Status bar:** with an agent session, its title and state use the same clock, question mark and check icons as the
  Console list. Working uses the accent color; waiting pulses in the warning color with the same rhythm as unread
  Output; done uses the success color. Idle is neutral and exited is dimmed. The tooltip includes the agent's note
  and a click hint; clicking shows the session. The *Agent control off* badge remains a warning.
- **Agent layout** (Window → Layouts): the Viewport takes 60% of the width beside a tall 40% column shared by Console
  and Inspector, with Console selected in front. The Viewport remains larger at 1440×900 and 2560×1440.

Text sent to a session that is still starting waits until the CLI's output settles, so a just-started agent receives
it in its prompt.

## Preferences

Layout, last custom layout, visibility, scale, favorites, recent opens, and Output preferences are in
`UserDataDirectory()/EngineEditor/preferences.json` (separated by build directory + project name). The agent CLI choice,
accepted setup and the Agent control switch belong to the user and apply to every project. Player progress / settings
files are unaffected.
