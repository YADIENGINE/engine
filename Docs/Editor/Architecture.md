# Editor architecture

| Layer | Target / source | Responsibility |
| --- | --- | --- |
| Public API | EngineEditorApi / Engine/Editor/Api | Metadata, parameters, inspection; knows no ImGui/IPC |
| Support | EngineEditorSupport / Internals | Project recipe, protocol, process, viewport, preferences |
| Worker runtime | EnginePreviewRuntime / Preview | Game-based preview and session |
| Shared UI | EngineToolUi / ToolUi | Theme, fonts, ImGui context, general widgets (shared with launcher) |
| UI | EngineEditorUi / Ui | Shell, docking, panels, and editor-specific widgets |
| Application | EngineEditor / Host, Application | Windows processes and UI lifecycle |
| Project worker | consumer target | Game View/model and explicit catalog registration |

There is no Engine → Game dependency. Normal games do not link Editor API or ImGui. `ENGINE_BUILD_EDITOR` requires
Windows and runtime; `ENGINE_BUILD_EDITOR_TESTS` can enable headless protocol/state tests with runtime disabled.
Dependency versions and IPC limits are in [API](Api.md).

## Worker flow

The editor first starts the worker in catalog mode; the registry supplies only metadata, without setting up
GL/fonts/audio. Opening a preview makes RunPreviewHost set up services and PreviewGame; Game::Run is the only loop.
PreviewScene manages the content instance through ObjectList. Reset replaces the instance.

- **Snapshots** (`Internals/Session/WorkerSnapshots`): workers run from an immutable copy of each successful build, not
  from build output (`<worker>.live-<n>.exe`, beside the output: Assets and the working folder resolve identically).
  Builds never write to a running executable; unused copies are removed. Debug symbols remain in build output and match
  the latest snapshot.
- **Worker instances** (`Application/PreviewWorker`): process + its own session (control channel, frame transfer) +
  startup / shutdown timing. Several can coexist: active preview, its replacement, and retiring instances. Stop, Open,
  and changing previews retire the old instance without waiting; if a retired instance does not exit within 5 s, its
  process tree is terminated.
- **Build & Refresh** (`Internals/Session/RefreshState`, `Application/PreviewBuild`): the running preview remains usable
  throughout the build. A successful build is snapshotted; if a preview is open, a new instance starts beside the old
  one with the same state (`PreviewOpenState`: preview, parameters, pause, speed; camera is host-owned) and replaces it
  when ready. If the build or new instance fails, the old preview never stops. Without an open preview, the new build's
  catalog is read. If the selected preview is absent from the new catalog, the catalog is accepted and the preview
  stops.
- Presentation time is not transferred: game state cannot be derived from time (scenes advance through Update), so it
  restarts as on Reset.

IPC protocol v8 carries focus loss / pause / input timeout as cancellation instead of normal release. Profiler messages
carry generation/capture IDs; records are not published until bounded chunks are complete. The public snapshot's
identified touch data and empty positions outside the canvas are also transferred. The host maps private window samples
to canvas coordinates with the Input normalizer; the worker consumes the same public contract. Actions accumulated
before cancellation are cleared; subsequent new presses arrive after the cancellation notice. Host and worker must
update together to the same protocol version; the project recipe version does not change.

## Tuning

The only write path for tuning files ([Tuning](../Tuning.md)) is the file itself; the editor does not send values to
workers. `Internals/Session/TuningWorkspace` reads the build's `Tuning/TuningFiles.json` list (CMake
`engine_add_tuning`) and files, refreshes on external changes, patches the current file on disk with `PatchTuningValue`,
and writes atomically. In development builds, the worker watches files itself (`PollTuning`); `PreviewHost` forwards
live tuning reports to the host in `tuning` messages (`PreviewTuningStatus`: source, state, line, text). State belongs
to the active worker: it resets when the worker changes; during Build & Refresh, the new worker's reports switch with
it. Release-built workers neither read live nor send reports. Presentation is `Ui/Panels/Tuning/TuningPanel`.

## Control gateway

Tools reach an open editor through a local named pipe (`Internals/Protocol/EditorControlProtocol`,
`Internals/Platform/Windows/EditorControlPipe`, `Application/EditorControlService`). While running, the editor announces
the pipe name in `EngineEditor.control.json` in the build directory (`version`, `pipe`, `pid`); on exit, it removes the
file if it still owns it. Messages are one JSON object per line (UTF-8, `\n`); version is `EditorControlVersion` (1). A
client sends a request and reads until `result` or `error`; only one client connects at a time.

| Request | Responses |
| --- | --- |
| `{"version": 1, "command": "refresh"}` | Starts Build & Refresh (joins if already running); attempt lines `{"type": "log", "level", "source", "text"}`, then `{"type": "result", "result": "succeeded" \| "build-failed" \| "refresh-failed" \| "canceled"}` |
| invalid / unknown | `{"type": "error", "text"}` |

If the launcher starts the editor with `ENGINE_LAUNCHER`, the File menu offers "Back to project list"
(`Host/ProjectListLauncher`; [ToolsContract](../ToolsContract.md#environment-variables)).

Its client is `python Engine refresh` ([ToolsContract](../ToolsContract.md)). The launcher and agent API add new
commands to the same gateway.

## Presentation clock and playback

- Visual time is read through `Painter::Time()`. Before drawing each frame, `Game` supplies virtual
  `Game::DrawSeconds()` to the Painter context. In games it is real time; in workers, the `PreviewGame` override
  supplies the preview scene's scaled clock, reset to 0 on Reset; custom Game adapters supply their flow clock.
- Views do not keep or forward clocks or call `GetTime()`. Drawing that advances its own time (portraits, intros) passes
  time as a `float time` parameter.
- Pause freezes Update and presentation time; drawing, communication, parameters, and actions continue. Speed (0.1×..4×)
  and Step (1/60 s) apply to both real-time menus and game time through `Game::FrameSeconds`.

## Controls and inspection

- Settings are `PreviewParameter`, actions `PreviewAction`; Inspector groups / descriptions are authored in content
  definitions.
- `CaptureParameters` reports authored values changed by actions or product input; `MergePreviewCapture` validates the
  whole report and applies it atomically; `OnReset` recipes remain staged.
- `PreviewInspection` carries read-only values, tables, focus area, and control visibility; it does not affect the
  recipe and is sent at most 10 Hz.
- Fit / Focus / 100% / pan / zoom use the same canvas transform for input; points outside the panel are rejected.



## UI layers (EngineEditor)

- Host owns Game / ImGui lifecycle, the GPU image bridge, and input routing; the ImGui context and image texture use
  RAII cleanup while the GL context is alive.
- Theme, fonts, ImGui context / scale, and general widgets are the tools' shared layer: [ToolUi](../ToolUi.md)
  (`EngineToolUi`; also used by the launcher). The editor creates the context with docking enabled and keyboard
  navigation disabled (preview controls share the keyboard). Editor-specific parts are in `Ui/Style/EditorTheme` (state
  colors / labels, `EditorGlyph`) and `Ui/Widgets/EditorWidgets` (glyph icon button, preview parameter).
- `Ui/Widgets/InspectorWidgets` is the shared Inspector presentation for all previews: plain collapsible section
  headers, shared label / value / reset columns across the panel; in narrow panels, labels above full-width controls,
  with one line for checkboxes that fit. Reset appears on hover / focus for unchanged fields; reserved space stays
  fixed. Choice groups use full panel width below the label at every width. Column count decreases according to text
  measurement; buttons stay single-line. Application time is summarized per group; mixed groups use markers / tooltips.
  `PreviewReadoutKind` distinguishes live numbers, status, and wrapped explanations. Header descriptions are under `?`,
  and small previews put search in a dropdown button. `advanced` groups start collapsed. Search and Modified only filter
  property presentation; they do not change authored groups, application time, or visibility contracts. Explicit
  `primary` metadata puts the main action in a fixed footer; settings and live information scroll in a separate child
  area. Secondary actions use two columns when wide enough, one in narrow panels.
- `Application/EditorController` manages processes, IPC, and preview commands. Panels know only `EditorSnapshot` and
  `EditorUiActions`; UI commands apply after all panels draw.
- `Application/EditorCommandState` resolves availability and blocking reasons in one place. Host-owned
  `EditorCommand::Step` uses the existing playback message; public `Preview*` API and worker protocol stay unchanged.
- `Ui/Commands` shares the command identity / label / icon / shortcut table with menus, toolbar, and command palette.
  The main Open / Stop / Cancel action derives from session state. Input consumed by shortcuts and the palette is not
  sent to workers in the same frame; preview control shortcuts are blocked during popups and text editing.
- `Ui/Shell` manages menu / global toolbar / status bar; `Ui/Workspace` manages docking, panel registration, Develop /
  Inspect / Profile layouts, and temporary maximization. Panel IDs are independent of visible titles; visibility and
  default dock positions are in the shared `EditorPanelDefinition` table. DockBuilder's internal API is used only in
  Workspace. Switching to a preset layout preserves the last custom layout; saved user layout is retained on startup,
  with Viewport open and in front. Window manages panel visibility / layouts, View manages display options.
- `Ui/Panels` owns local tools: Viewport's local View display / zoom menu, Output's single Copy menu, Profiler's
  advanced measurement settings and Export menu. Output / Profiler / Console are closed by default; the bottom status
  bar toggles Output and Console independently, showing process state and unread error / warning notices.
- `Internals/Diagnostics` and `Internals/Settings` know no ImGui / raylib. Preferences are stored atomically under
  `UserDataDirectory()/EngineEditor/preferences.json` (layout, last custom layout, visibility, scale, favorites,
  history, Output).
- A new panel is a value component in its own folder, added to shared panel registration. A new process command gets
  behavior / availability in Application and presentation metadata in Ui/Commands; menus, toolbar, and palette share the
  same contract. Do not add empty services or unused public API for unsupported capabilities; dependencies stay in the
  [UI development list](TODO.md).

The Profiler panel provides Live / Benchmarks / Compare. The headless module and optional runtime observer are
independent of the editor. During measurement, the worker stops image transfer; the host drops to 10 Hz.
FPS/VSync/render policy belongs to the measurement session and is restored afterwards. Details: [Profiler](Profiler.md).


Project name and build directory separate editor preferences. Worker options are transient. In real-game adapters,
progress and telemetry isolation belong to the project; details in [CreatingPreviews](CreatingPreviews.md).