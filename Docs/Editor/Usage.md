# Editor usage

## Window

Saved dock layout and panel visibility are retained; Viewport starts open and in front on every editor launch. On first
launch, Develop shows Scenes, Viewport and Inspector; Output, Profiler, and Console are closed. The bottom status bar has Output and Console buttons.

The toolbar's **Play game** button, immediately right of Build, uses a pixel invader icon. It builds the
standalone game in Debug through the project tools, then opens it in its own window after a successful build.
Build output and errors appear in Output; cancellation or a failed build never launches the game. It requires
the recipe's Python, engine and source paths plus a game configuration in `Project.json`.

Quick open (Ctrl+P) searches scenes, their choices, graphics, sounds and commands. Its toolbar hint shortens to
**Quick open** when space is limited. Quick open and the Scenes, Inspector/tuning and Assets searches match every
space-separated word in any order, ignoring case; panel result ordering is preserved.

- **Left (Scenes):** Favorites and Recent filters, search beside them, categories with their scene count. Single-click
  runs content. A newly selected scene opens its category and scrolls into view once; collapsing
  it afterwards stays collapsed. A row's star appears on hover (always on favorites) and toggles the favorite. Recent
  keeps the last 10 successful opens.
  Active Favorites / Recent scopes are named above the results. The search's **x** clears only its text and returns
  typing focus; **Reset filters** clears the text and scope together. Empty results explain the active scope.
- **Center (Viewport):** the worker's actual image on a neutral background. The dropdown at its top left ("Fit · 56%")
  names the view mode and the image's real scale, screen pixels per canvas pixel. Fit shows the whole canvas, Focus
  content shows the area reported by content, and Actual size shows it at 100%. Zoom in the same menu or Ctrl+wheel
  multiplies the mode's scale; middle-button dragging pans; Reset pan and zoom resets view settings. Without Ctrl, the
  wheel goes to the product. The maximize button or double-clicking the strip between them enables temporary focus mode.
  Closed, loading, or failed editor scenes show a state explanation and an available run / retry / cancel action; with
  no scene or a stopped one, also the last five scenes opened, one click from running again, and with no scene the
  shortcuts. A short viewport drops the shortcuts first, then the recent scenes. Playback is only in the global
  toolbar, at its left, with the search box (quick open, Ctrl+P) in the middle and speed, Mark, Build and Play game at
  its right. Mark needs no running scene: a region on a scene's image marks the scene, anywhere else the editor window.
- **Right (Inspector):** the header's `?` opens the description by tooltip or click. Property search and the Modified
  counter find changed fields; with at most six visible fields, the Search button opens search. An accent dot marks a
  value different from default; Pending marks a value awaiting worker response, not a setting applied on the next
  action. Reset icons appear on changed fields, or on hover / keyboard focus for others, resetting only that field. The
  primary action is fixed at the panel bottom; settings, secondary Actions, and Live information scroll in their own
  area. Choice controls are searchable; large integers such as seeds can be typed directly. Sliders are filled bars with
  the value written over them; Ctrl+click types a value. Up to four choice buttons that fit share a row with their
  label; more use full panel width below it. Normal controls move below labels in narrow panels; checkboxes stay on the
  label line when they fit. Normal fields share a label column; button and field names truncate instead of wrapping
  letter by letter, with full text in tooltips. Advanced groups start collapsed; search and Modified filters expand
  matching groups. Live information shows numbers right-aligned in technical font, short statuses as badges, and
  explanations wrapped.
  Search's **x** keeps Modified active; **Reset filters** clears both. Empty results say when Modified is limiting
  the matches and offer the corresponding clear/reset actions.
  Two kinds of values share these widgets, and each says which it is on hover. **Scene controls** (the scene's
  parameter groups) last for the session and are never saved; their reset restores the default. **Tuning files**
  ([Tuning](../Tuning.md)) follow them, one section per file the running scene has read, and are saved: a dragged number
  previews in the scene at once and is written to the file as one line when the drag ends; typing commits on Enter or
  when the box loses focus. Modified compares tuning with the last commit (git HEAD), and their reset button or
  right-click → Revert to committed writes it back. Right-click also opens the field's line in the code editor or sends
  it to the agent. Ctrl+Z / Ctrl+Y undo and redo the Inspector's tuning writes while the file still holds what they
  wrote. Lists and tables have `+` / `-` to change their length, which needs Build & Refresh. A section's badge is the
  scene's state for the file: Live, Not applied (line and reason), Build needed (with Build & Refresh), Unreadable; a
  file no build has seen yet is Not built. Any other tuning file opens from the list under the sections, from the list
  shown when no scene is selected, or by clicking its tile in Assets; the Inspector then shows only that file, with a
  button back to the scene. Picking a scene in Scenes returns to it too.
- **Output:** one row of controls above the lines. `Problems` lists the latest build's compiler, linker and CMake
  messages, errors first and without repeats; a click opens the file at the line (VS Code when `code` is on PATH,
  otherwise the file's default application), *Fix* pastes that error into the agent session and *Fix all with agent*
  sends them all and presses Enter. `Last build` shows output from the latest Build & Refresh click
  until the scene is ready, fails, or is cancelled; `Full` shows the last 1000 lines from editor, worker, and all
  attempts. Under the row, `Last build` names the attempt's start time, duration, result, and build exit code. Info /
  Warning / Error counters, search, and Copy Visible apply to
  the selected scope. Click selects one line, Ctrl+click multiple lines, Shift+click a range. With the line area
  focused, Ctrl+A selects visible lines, Ctrl+C copies selection; right-click also offers Copy Selected / Copy Visible.
  Double-click opens the message in read-only details with mouse text selection. Normal Ctrl+A / Ctrl+C work in search
  and detail text fields. Follow stops on scrolling up or selecting a line. Clear clears only the Full buffer; the last
  attempt and files remain. `Open log` opens the selected scope's full file. Files are kept per session and attempt
  under `editor-logs/` in the build directory; they are not deleted automatically. The last-attempt buffer is limited to
  10000 lines / 8 MiB; on overflow, the first error is retained and a reference to the full file appears. A failed build
  brings Output forward once, on Problems; a failure with no compiler messages (a worker that would not start) opens
  Last build at the first error instead, with earlier search / level filters set aside until the scope changes. It waits
  while a scene is maximized or text is being typed, and an attempt started through the control endpoint reports to its
  client; the status bar shows the error count until the next build either way, and a click on it opens Problems. While
  building, the status bar shows Ninja's count and the time so far ("Building 12/48 · 14 s").
- **Assets:** the asset library: back and search across the top, a folder tree on the left (sets are rows too), a tile
  grid on the right, and tile size at the bottom (or Ctrl+wheel). A click on a graphic opens it in the Viewport; a click
  on a folder or set enters it. Back (Alt+Left) undoes the last move or opened graphic, in order. A click on a sound
  loads it into Audio Preview and plays it; a second click on the playing sound stops it. Once a worker has reported
  the project's audio catalog, a sound the catalog does not list is dimmed, and the tooltip names the catalog entries
  that play it. Right-click for Copy usage, Copy path, Open code and
  Show in Explorer. The + button or a right-click creates a folder or graphic; dragging onto a folder moves it, updating
  includes. The Engine root is read-only. A graphic not built yet is dimmed; Build & Refresh also renews thumbnails.
  Registering assets: [Api](Api.md#graphic-assets).
  Search covers all project and engine asset folders; **Searching all asset folders** appears above its results.
  Clearing search returns to the current folder and restores typing focus (Ctrl+F focuses the asset search).
- **Audio Preview:** the sound picked in Assets, beside it in the bottom dock by default. It shows the file name,
  folder, length, format, size, sample rate and channels; whether the audio catalog uses it, as `Effect <name>` or
  `Music <name>`, or `#<id>` when the catalog entry has no `name`; and a waveform (a plain bar until it has been decoded
  in the background). Click or drag on the waveform to seek. Below it: previous sound in the folder, play/pause (Space),
  stop, next sound, loop and volume. It plays in the editor process from the source file, so a new sound needs no Build
  & Refresh, and it leaves the viewport and running scene alone.
- **Console:** a real PowerShell session at the project root, with the sessions in a named list at the right that can be
  resized. Drag to select visible text and copy with Ctrl+C /
  Ctrl+Shift+C or right-click Copy. Without selection, Ctrl+C interrupts the running program; Ctrl+V pastes, Escape
  clears selection. Wheel, the scrollbar and Shift+PageUp/PageDown browse shell history; typing returns to live output.
  In live applications such as Claude and Codex, wheel input follows the requested mouse protocol; full-screen
  applications without mouse reporting receive cursor keys. Shift+wheel forces local history browsing, and fractional
  touchpad movement accumulates. Double-click word selection is not yet available.
- **Profiler:** Live / Benchmarks / Compare. Opens when a benchmark example is selected; warmup / measurement frames /
  repetition pairs are under Advanced benchmark settings. Glow override is one choice group; highlighting shows the last
  requested value. Compare's Export menu offers JSON and Markdown. Details: [Profiler](Profiler.md).
- Panels can be docked, combined into tabs, closed, and reopened from View → Panels. A scene's values, its language
  among them, are set in the Inspector; before the scene opens they become part of the recipe it opens with. Interface
  scale (75% / 100% / 125% / 150%): the default 100% matches the previous 125% size; UI and technical text fonts scale
  with user scale and monitor DPI.

Under View → Layouts, Develop opens general scene layout, Inspect a wider Inspector, Profile a Profiler-focused layout
and Agent the scene beside a tall Console. Last custom layout restores the last manual layout; selecting a preset does
not erase it. Bottom-bar Output / Console buttons only toggle their respective panel; multiple panels can stay open.
View → Panels → Toggle last bottom tool or Ctrl+J toggles the last-used bottom panel without affecting others.
Unread errors or warnings flash the Output button; viewing Output clears the notification. Highlighting uses the theme's
error or warning color; errors take priority.

## Menus

The menu bar, the command palette, Help → Keyboard shortcuts and the keyboard itself read one list of actions and one
table of keys, so an action has the same name, shortcut and reason for being unavailable everywhere, and a shortcut
always does what its menu item does. A disabled item says why in its tooltip.

| Menu | Holds |
| --- | --- |
| File | Quick open, Back to project list (when a launcher started the editor), Exit |
| Scene | Run / Stop / Cancel (F5), Pause / Resume, Step one frame, Reset |
| View | Command palette, Focus viewport, Panels, Layouts, Interface scale, viewport performance metrics |
| Build | Build & Refresh, Build & Play game, Cancel build |
| Git | Changes, Incoming commits, Fetch, Pull, Push, Update submodules; in a project that is no repository yet, Changes offers to create one |
| Agent | Send scene / build failure, Ask agent to fix build, Show agents, New agent, open the agent CLI, New shell, Agent control, Visual checks, Agent CLI |
| Help | Keyboard shortcuts, Editor guide, About |

Editor guide opens this page: the engine's own `Docs/Editor/Usage.md` when the engine folder has it, else the page for
this engine version in its repository. About names the engine and its version.

## Toolbar

The global toolbar has one primary action: Run, Stop, or Cancel according to state. Pause / Resume is a separate time
control. The primary action and Build & Refresh have labels when space allows; in narrow areas, secondary controls move
to an overflow menu. Tooltips show name, shortcut, and the reason if unavailable.

| Button | Meaning |
| --- | --- |
| Run / Stop / Cancel | Runs selected content when closed; closes the scene when ready or paused; cancels catalog reading, startup, building without a scene, or benchmarking. Cancel an ongoing build with an open scene through Build → Cancel build |
| Pause / Resume | Freezes Update and presentation time; drawing, parameters, actions, and Reset remain available |
| Step | Advances 1/60 s while paused |
| Speed | 0.1×..4× |
| Reset | Creates a new instance from the accepted recipe (clock starts at 0) |
| Build & Refresh | Builds the worker target; the scene keeps running throughout. A successful build takes selection, values, pause, and speed and replaces the old one when ready; errors leave the scene unchanged. From terminal: `python Engine refresh` |

Reset recreates the scene in the same worker process. Use Run to restart the worker process.

| Shortcut | Action |
| --- | --- |
| F5 | State-dependent primary action |
| F6 | Pause / Resume |
| F7 | Single frame while paused |
| Ctrl+R | Reset |
| Ctrl+Shift+B | Build & Refresh |
| Ctrl+P | Quick open: scenes and their choices, graphics, sounds, commands |
| Ctrl+Shift+P | The same window, commands only |
| Ctrl+` | Toggle the Console |
| Ctrl+Shift+G | Toggle Git changes |
| Ctrl+Shift+A | Open or close the Agents drawer, on the agent that waits for you (also from a terminal) |
| Ctrl+Shift+S | Send scene: to the drawer's chosen agent while it is open, else to the agent session |
| Ctrl+Shift+N | New agent (the drawer's **+** menu) |
| Ctrl+PageDown / Ctrl+PageUp | In the Agents drawer: the next / previous agent |
| Ctrl+1 … Ctrl+9 | In the Agents drawer: agent 1 to 9 |
| Ctrl+Shift+T | In the Agents drawer: Terminal ↔ Activity |
| Ctrl+J | Toggle last bottom tool panel |

The same list is available in the editor from **Help → Keyboard shortcuts**.

Quick open starts with the scenes opened last, then the commands; typing finds anything by any of its words, in any
order ("medusa boss"). Picking a scene runs it, picking one of its choices ("Boss flow · boss: Medusa") runs it with that
value, picking a sound plays it in Audio Preview. Select with up / down, run with Enter, close with Escape. Unavailable
actions show their reason. No shortcut runs while a popup is open. Playback / build shortcuts (F5, F6, F7, Ctrl+R,
Ctrl+Shift+B) also wait while you type or while the Console has the keyboard, and Ctrl+P while the Console has it; the
Console leaves every other shortcut in the table to the editor. Input consumed by the editor is not forwarded to the
scene.

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

## Source control

The right end of the menu bar always shows the project's git state: repository, branch, **Fetch**, and **Pull**,
**Commit** and **Push** with their counts. Hovering Fetch or Pull also tells when the last fetch was. Only the next sensible step is colored: conflicts, then Pull, Commit, Push. It
needs [Git](https://git-scm.com/downloads) on the machine and uses it as your terminal does, with the same sign-in, LFS
and hooks. A project not in version control gets **Set up Git**, which starts a repository with the template's ignore
rules; nothing leaves the computer until you publish.

- **Fetching** happens on opening, every 10 minutes, and on returning to the window (at most every 2 minutes). It never
  asks for anything, and the buttons' icons animate while it runs. New commits pulse Pull for 6 seconds and leave it tinted with the count; more commits pulse it again.
  A refused sign-in stops automatic fetching until you click **Sign-in needed**, since repeated failures can lock an
  account.
- **Pull** only fast-forwards, and hovering it lists what is coming. It waits for a running build, and builds wait for it.
  When it brings changes, the editor builds them. If your changed files overlap the incoming ones it stops and offers
  **Stash & Pull**. If both sides have commits, the button becomes **Merge** and asks first.
- **Commit** (Ctrl+Shift+G) opens the drawer over the left of the workspace without moving any panel. Nothing is ticked
  to begin with: only the files you choose are committed, even if someone else staged others, so sharing the folder with
  an agent or a teammate is safe. A file's name opens its change in the viewport's place; Esc returns to the scene.
  Pictures show before and after side by side, and sounds play either way. Right-click an untracked file for
  **Add to .gitignore**. Files of 100 MB or more that LFS does not track ask first, and the build folder is refused.
- **Conflicts** turn Commit into **Resolve**: choose **Mine** or **Theirs** per file (the only option for pictures and
  sounds), mark a hand-edited file **Resolved**, or use **Fix with agent**. **Cancel it** undoes the whole merge.
- **Push** becomes **Publish** for a branch the remote does not have; a project with no remote asks for its address.
  A push is refused while it records submodule commits that exist only on this machine, with **Push them too** offered.
- **Submodules** appear in the branch menu, each with its own counts, and that repository becomes the one the bar acts on.
  A pull moves a submodule forward on its current branch rather than detaching it, and leaves it alone when it has
  uncommitted or unrecorded work. A commit on a detached HEAD is refused until you choose a branch.

There is no force push, reset or discard here; those stay in the Console, where they are a deliberate act. Git's own
words for any failure are in Output.

## Agent workspace

The editor runs the developer's own agent CLI (Claude Code, Codex or any command) in a Console session and hands it
editor context; it never calls a model itself. The agent drives the editor back through `python Engine ctl`
([Automation](Automation.md)), and the project opts in to the engine's agent rules through `Engine/AgentDocs/Agent.md`
([Using an AI agent](../ProjectSetup.md#using-an-ai-agent)).

- **Console sessions:** the Console holds any number of sessions: shells and agent CLIs, named *Shell*, *Claude*,
  *Codex* (*Claude 2* for a second one). A narrow list on the right of the terminal shows each session with a terminal
  or robot icon; drag its left edge to resize it. Click a row to select it; the selected row is highlighted. Hover
  shows its title, state and current activity. A close button appears on hover; right-click also offers *Close*.
  Labels and terminal/robot icons follow live Claude and Codex executables in that session, checked every half second.
  Starting either CLI from a shell changes the row to Claude/Codex; exiting it restores Shell, including sessions opened
  from the agent menu. Custom commands retain their configured identity. Old notes do not carry into the next application.
  The **+** button at the top (*Add terminal* on hover) offers Shell, Claude, Codex and *Custom agent*, with the named
  CLIs' commands alongside them. Choosing an agent makes it the one agent requests start next. Each session keeps
  running while the panel is hidden; closing one ends its program (a running agent asks first). Agent requests
  (Send scene, Marks, ...) go to the selected session when it runs an agent, else to the agent session
  selected last; with none, the chosen agent starts. The first draw opens a shell when there is none.
- **Session state** (each session's icon): *working* while its program writes output for a request, *waiting for you*
  when it rang the terminal bell after your last input (agent CLIs ring it when they ask something), *done* when the
  output went quiet after a request, *idle* before anyone typed into it, *exited* when the program ended. An agent's
  own `python Engine ctl note TEXT --state working|waiting|idle` from that session overrides the guess until you type
  again; its text, else the terminal title the CLI sets, is what the session shows on hover. Each session's program
  gets `ENGINE_EDITOR_SESSION`, which every `ctl` request sends along. Working shows a clock; waiting shows a pulsing question
  mark in the warning color; done shows a green check. Idle has no state icon, and exited rows are dimmed.
- **Agents drawer** (a click on the agent strip, Agent → *Show agents*, Ctrl+Shift+A): where you work with agents. It
  hangs under the strip at the workspace's top right, about a quarter of the screen, over the panels and without moving
  them, and stays open until you close it (✕, Ctrl+Shift+A, the strip again, or Esc while its terminal does not have
  the keyboard: Esc belongs to the agent, which stops on it); drag its bottom-left corner to resize it. Its keys work
  while you type in its terminal; they are in the shortcut table above.
  - *Header:* how many agents work, wait for you or are done; who holds the editor ([Several agents](Automation.md#several-agents))
    and for how long it has been idle; the *Agent control* / *Visual checks* warnings.
  - *Chips:* one per agent session in the Console's order, so a chip never moves while you aim at it: its state's dot,
    its name and a lock on the holder; a waiting agent's chip glows. Hover shows its latest note or tool line. Shells
    stay in the Console. Agents run outside the Console share one chip, *Outside the Console* (a ring for a dot), while
    they have an editor action, a note or the hold. **+** starts Claude, Codex or a custom agent; right-click a chip
    for *Open in Console*, *Restart* and *Close session*.
  - *Terminal:* the chosen agent's live terminal, with the keyboard when the drawer opens; you talk to the agent here.
    While the drawer shows it, the Console shows "… is open in Agents" with *Show here* instead of drawing the same
    terminal twice.
  - *Activity:* what that agent did, newest first, filtered by *All*, *Editor* (changes with *Undo*, captures with
    *Open*, commands and notes) or *Tools* (its last 100 tool lines from hooks).
  - *Send scene* pastes the open scene into this agent's prompt; an exited agent offers *Restart*; the ⋯ menu has *Open
    in Console* and *Close session* (a running agent asks first).
  - *No agent yet:* the drawer offers Claude Code, Codex and a custom command, the one in your preferences first;
    Enter starts it.
- **Opening an agent:** the drawer's **+**, its empty state, the Console's add button or *Open* in the Agent menu or the palette start
  the chosen CLI in the project folder (or select its session) and show it in the Agents drawer. The session's shell stays open after the CLI exits. On first use, a setup modal lets you choose
  Claude Code, Codex or Custom, checks the executable on PATH (or at its full path), and offers an *Install...* link
  for the named CLIs. Custom shows a command field with an argument hint. *Check again* rechecks installation; if PATH
  changed, restart the editor or use a full path. *Start* accepts the choice and opens the session; *Cancel* leaves
  preferences unchanged. Setup also opens if the command is missing, with a message in Output. Context sent during
  setup waits for Start; Cancel discards that pending context. *Copy hint* copies: "Let the agent know the engine:
  add @Engine/AgentDocs/Agent.md to your CLAUDE.md (or AGENTS.md)".
- **Agent menu** groups Send context and Settings. Open agent and New shell are available through the Console's add
  button and remain in the command palette as "Agent" entries, alongside the context commands:
  - *Send scene* pastes the open scene's id, playback time, changed parameters and a capture of the frame
    (`editor-captures/scene-*.png`) into the agent session, shown in the Agents drawer, without Enter: type your
    request after it.
  - *Send build failure* pastes the last failed Build & Refresh with its error lines; *Ask agent to fix build* also
    asks for a fix and presses Enter, as *Fix all with agent* in Output → Problems does. Disabled context commands
    explain why in a tooltip.
  - *Show agents* opens the Agents drawer.
  - *Agent control* (on by default): off, tools may observe the editor (status, list, screenshot, inspect, logs) but
    every command that changes it (open, set, action, playback, seek, frames, refresh) is refused. The status bar shows
    "Agent control off".
  - *Visual checks* (on by default): off, tools may not capture screenshots or frame grids; the agent tells you what
    to look at instead. Saved with Agent control.
  - *Agent CLI* uses radio choices for Claude Code (`claude`), Codex (`codex`) or Custom; the command field is shown
    only for Custom. *Agent control* is a checkbox with a tooltip explaining read-only access.
- **Output:** right-click → *Send Selected to Agent* pastes the selected lines.
- **Inspector:** right-click a parameter's label or value → *Send to Agent* pastes its current value, range or choices
  and description, without Enter. This is also available for disabled parameters.
- **Marks** (toolbar Mark, Alt+drag) go to the agent session when there is one, in the Agents drawer.
- **Agent strip** (toolbar, far right): it speaks of the session that waits for you, else of the one agent requests go
  to: its state in color, its latest activity and how many came before ("+47", "99+" past two digits; filled until you
  have opened Agents). With two or more agent sessions it also shows a dot per session in its state's colour (the
  holder's dot has a ring, more than five show as "+N") and names the session; a narrower strip keeps the dots only. Working uses the agent's own hue with
  the walking cells and shows what the session says it is doing; waiting pulses in the warning color; done stays green
  for a few seconds, then neutral. A new activity lights the strip once. An older entry shows its age ("4m"). In a
  narrow window the text shortens, then the tools move to overflow, and last only the mark and the count remain.
  A click toggles the Agents drawer, on the waiting session when there is one. With one session, a small lock appears
  when another client holds the editor. Without a
  Console session, the strip still shows activity from agents elsewhere, in neutral. The status bar keeps the *Agent
  control off* warning.
- **Several agents:** one session at a time may change or capture the editor; another one's such command is refused
  until the holder has been idle for 60 s ([Several agents](Automation.md#several-agents)). Your own use of the editor
  is never refused and ends an agent's hold.
- **Agent layout** (View → Layouts): the Viewport takes 60% of the width beside a tall 40% column shared by Console
  and Inspector, with Console selected in front. The Viewport remains larger at 1440×900 and 2560×1440.

Text sent to a session that is still starting waits until the CLI's output settles, so a just-started agent receives
it in its prompt.

## Preferences

Layout, last custom layout, visibility, scale, favorites, recent opens, and Output preferences are in
`UserDataDirectory()/EngineEditor/preferences.json` (separated by build directory + project name). The agent CLI choice,
accepted setup and the Agent control and Visual checks switches belong to the user and apply to every project. Player progress / settings
files are unaffected.
