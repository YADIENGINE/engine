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
  maximize button or double-clicking the view summary enables temporary focus mode. Closed, loading, or failed previews
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
- **Output:** `Last Build Try` shows output from the latest Build & Refresh click until the preview is ready, fails, or
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
  first error message together. Show Output exits maximized preview, brings Output forward, opens Last Build Try, and
  scrolls to the first error. In this view, previous search/level preferences are temporarily removed; switching scope
  restores saved preferences.
- **Console:** a real PowerShell session at the project root. Drag to select visible text and copy with Ctrl+C /
  Ctrl+Shift+C or right-click Copy. Without selection, Ctrl+C interrupts the running program; Ctrl+V pastes, Escape
  clears selection. Scrollback and double-click word selection are not yet available.
- **Tuning:** the project's `*.tuning.json` files ([Tuning](../Tuning.md)); the list comes from the build's
  `Tuning/TuningFiles.json`, with new files appearing after the next configure. File selection is at the top, followed
  by the preview's latest state for that file: Live, Not applied (invalid file; line and reason), Build needed (field /
  length changed; Build & Refresh button), Unreadable. If the preview does not use the file or is not open, this is
  stated. Value fields are label / box rows; tables are grids. Type numbers, vec2, range, and colors (`#RRGGBB[AA]`) and
  commit with Enter; leaving a box without Enter reverts the edit; checkboxes write on click. Out-of-bounds or invalid
  values are not sent; the box stays red, with the reason in a tooltip. An accepted value is patched as a single value
  into the current file on disk and written atomically (external changes are not overwritten); if the file is currently
  invalid, no write occurs and Output gets a warning. External edits (by hand or AI) refresh the panel. Add rows /
  fields in the file, not in the panel.
- **Profiler:** Live / Benchmarks / Compare. Opens when a benchmark example is selected; warmup / measurement frames /
  repetition pairs are under Advanced benchmark settings. Glow override is one choice group; highlighting shows the last
  requested value. Compare's Export menu offers JSON and Markdown. Details: [Profiler](Profiler.md).
- Panels can be docked, combined into tabs, closed, and reopened from Window. Preview → Language changes the selected
  preview's language; it can be chosen before opening a preview and is used in the opening recipe. Language control is
  not shown in Inspector. The menu always opens and explains previews without language support. The top View menu
  manages Focus preview / Restore workspace, UI scale (75% / 100% / 125% / 150%), and preview performance metrics. The
  default 100% matches the previous 125% size; UI and technical text fonts scale with user scale and monitor DPI.

Under Window → Layouts, Develop opens general preview layout, Inspect a wider Inspector, and Profile a Profiler-focused
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
| Open / Stop / Cancel | Opens selected content when closed; closes the preview when ready or paused; cancels catalog reading, startup, building without a preview, or benchmarking. Cancel an ongoing build with an open preview through Cancel build in the command palette |
| Pause / Resume | Freezes Update and presentation time; drawing, parameters, actions, and Reset remain available |
| Step | Advances 1/60 s while paused |
| Speed | 0.1×..4× |
| Reset | Creates a new instance from the accepted recipe (clock starts at 0) |
| Build & Refresh | Builds the worker target; the preview keeps running throughout. A successful build takes selection, values, pause, and speed and replaces the old one when ready; errors leave the preview unchanged. From terminal: `python Engine refresh` |

Reset recreates the preview in the same worker process. Use Open to restart the worker process.

| Shortcut | Action |
| --- | --- |
| F5 | State-dependent primary action |
| F6 | Pause / Resume |
| F7 | Single frame while paused |
| Ctrl+R | Reset |
| Ctrl+Shift+B | Build & Refresh |
| Ctrl+P | Project search |
| Ctrl+Shift+P | Command palette |
| Ctrl+J | Toggle last bottom tool panel |

Search preview actions, panel visibility, and layouts in the command palette; select with up / down, run with Enter,
close with Escape. Unavailable actions show their reason. Playback / build shortcuts do not run during text editing or
popups; input consumed by the editor is not forwarded to the preview.

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

Navigation, hover, selection, and scroll input in the previewed product UI are preserved. Clicking the image gives
keyboard focus; Inspector text boxes and popups consume input. Dragging started in the image preserves release outside
the panel. Focus loss clears held keys.

## Real game flows

If the catalog offers such an entry, the worker can run production scenes through a custom Game adapter. Startup options
and in-memory profile policy are project-defined. The product's Back action belongs to game navigation; toolbar Stop
closes the worker. The host keeps OS window and capture size fixed.

## Preferences

Layout, last custom layout, visibility, scale, favorites, recent opens, and Output preferences are in
`UserDataDirectory()/EngineEditor/preferences.json` (separated by build directory + project name). Player progress /
settings files are unaffected.