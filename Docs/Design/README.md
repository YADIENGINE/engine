# Engine design record

Decisions and the reasons that keep them from being argued again. How things are used today is in the rest of `Docs/`;
each record links its usage page. This folder is left out of engine copies (`export-ignore`), so it serves people and
agents working on the engine itself. A new decision gets the next `Dnn` number; when one replaces another, edit the old
record instead of keeping both.

## Goals

1. **Visuals come from code.** Only fonts, sounds and platform packaging files are imported ([D1](#d1-assets)).
2. **Most work is code.** The editor does not build scenes or place objects; it shows the result of code, lets values
   be tuned and helps debugging.
3. **Built for working with AI agents.** Engine source ships in every project so an agent can read how it works;
   everything is diffable text.
4. **One product:** a launcher (project manager and CLI) and an editor compiled from each project's own engine.

## Decisions

### D1. Assets
Visual content is produced by code. Importable: fonts, sounds (external sources allowed), platform packaging files.

### D2. Tuning files
Values tuned in the editor live in `X.tuning.json` beside their class. The JSON is the only source; a struct is generated
from it. Development reads values live, Release compiles them in. Opt-in. Visual and balance values use the same system.
Usage: [Tuning](../Tuning.md).

### D3. Launcher plus an editor compiled per project
The launcher is prebuilt and changes rarely. Each project's editor is compiled from that project's `Engine/`, so editor,
worker and game always share one engine version and no protocol can mismatch. Projects on different engine versions live
side by side. The launcher-project contract (`Project.json`, `Engine/Tools` entry points) stays small and stable.

### D4. Engine copies in projects
- A user project holds a plain copy of an engine release in `Engine/`, read-only: it is documentation for the agent, not
  something to fix. A developer project has `Engine/` as a git checkout of the engine repository.
- `Engine/EngineManifest.json` is the hand-written identity (name, version, `toolsContract`, `repository`).
  `Engine/EngineCopy.json` is generated on import: `{"format": 1, "source": {"tag", "commit"}, "files": {path: "sha256:..."}}`,
  sorted `/` paths, every file except itself. The launcher uses it to detect edited files and to update cleanly.
- The files tell the project kind: `EngineCopy.json` means a copy; otherwise a git checkout means a developer project;
  neither means unknown origin. The launcher keeps no separate project setting. User mode never runs git.
- Developer mode is a plain switch in the launcher settings, with a warning; pushing is guarded by GitHub permissions.

### D5. Agent API
Commands that let an agent see the result of a change itself are a strategic priority and must not be made harder by
other decisions. The editor side is D20; [Agent API](AgentApi.md) holds the older headless plan.

### D6. One CLI; explicit worker refresh
- The launcher executable runs commands when given arguments. Its own commands are `<engine> launcher <job>`; everything
  else is passed unchanged to the project's engine (`python Engine <args>`), so new engine commands need no launcher
  change. The engine never adds a command named `launcher`.
- `build` compiles the game; `refresh` explicitly asks the open editor to build and replace its worker. They are
  separate commands, not an automatic redirect. Immutable worker snapshots keep the old process alive while the new
  build overwrites its output; the editor owns that worker build and its build-folder lock.
- No file watcher: building is an explicit trigger.
- Project discovery: `--project ROOT`, else the first folder upward with `Project.json`.
- The launcher reads `Engine/EngineManifest.json` directly, so its fields are part of the [tools contract](../ToolsContract.md).
  An unknown `toolsContract` exits with 2 and asks to update the launcher.

### D7. Python: embedded, tools run from the project's engine
The launcher ships embeddable Python; build, test, release and editor logic stay in the project's `Engine/Tools`, versioned
with the engine. Engine tools use the standard library only. NumPy/SciPy for sound generation are an optional download
in the launcher, run through `<engine> launcher python SCRIPT`.

### D8. MSVC, tools and dependencies
- The compiler is MSVC (VS Build Tools), installed by the launcher's system check; it cannot be redistributed. CMake and
  Ninja ship with the launcher, pinned with SHA-256 in a lock file; the launcher puts them first on the child `PATH`.
- Third-party sources are vendored under `ThirdParty/`, never fetched at build time, so a copied engine is complete and
  old projects keep building offline.

### D9. No hot reload; build beside, then swap
Code changes restart the worker and restore its state (scene, parameters, clock, pause, camera). The new worker is built
under a different name while the old one keeps running; a failed build leaves the old one and its preview. DLL swapping
is fragile in C++/raylib and AI-written code crashes often; a separate process protects the editor.

### D10. Visual assets: baking
Superseded by D22 for the catalog. Baking drawings into cached textures stays deferred until a measurement asks for it.
Background: [Visual assets](VisualAssets.md).

### D11. Spatial values are numbers
Positions, rectangles and paths in tuning are numbers. Later, if wanted: `point`, `rect` and `path` types with draggable
handles that write the JSON. Things whose count changes stay in code.

### D12. Debugging through existing tools
Usage: [Crash reports](../Debugging.md). No debugger of our own.

1. *Local reports also ship in Release:* text, versioned JSON and a small dump; no upload or symbol server.
2. *cpptrace 1.0.4 stays private:* ordinary exceptions retain throw frames through engine cleanup; terminate/abort use
   the best available fatal-handler trace. Handled errors are not crashes.
3. *A helper starts while healthy:* native handlers capture fixed exception/context data and signal it. DbgHelp,
   JSON and dump writing run outside the failed process, with a ten-second limit.
4. *PDB GUID/age identify symbols:* optimized Release and prebuilt libraries retain debug information. Player packages
   carry the helper; exact EXE/PDB archives remain separate and preserved.
5. *Worker snapshots own EXE/PDB/reporter together:* a lease protects the whole snapshot during execution and capture.
6. *Each tool owns its process reports:* Output, run/sim/refresh and test results expose current crash paths. `crash`
   resolves JSON without editing it. `build` only compiles and does not redisplay old crashes.

Left out: VS Code template tasks/launch and "wait for debugger" (remaining D12 work), Tracy, a general logger,
remote collection, arbitrary minidump analysis, and guarantees for pre-main/fail-fast/externally killed processes.

### D13. Windows desktop only

### D14. Project template
- "New project" installs no sample code; an empty `Game/` falls back to the starter.
- No AI file or settings in the template; projects opt in by referencing `Engine/AgentDocs/Agent.md` (D21).
- The launcher does not touch git in user mode.
- What stays out of a copy is listed in the engine's `.gitattributes` (`export-ignore`); imports use `git archive <tag>`.

### D15. Launcher
The launcher is its own product in `YADIENGINE/yadien`; its install, update, screens and packaging are documented there.
What binds the engine:
- Games never run from the launcher; each project builds and runs from its own `Engine/`.
- Launcher and editor share `ToolUi/` (target `EngineToolUi`, includes from the engine root). It is not in `Core/`, because
  games never link ImGui. Editor-only widgets stay in `Editor/`.
- The launcher sets `ENGINE_LAUNCHER` for engine commands; the editor shows "Back to project list" only when it is set.
- Tool UI text is English only.

### D16. Versions and releases
- `MAJOR.MINOR.PATCH`. In 0.x the MINOR digit is breaking. 1.0.0 comes when v1 is done and a game ships through the launcher.
- Breaking means a project that built on the previous version needs a manual change: public headers, types or functions
  removed or changed (`Internals/` excepted), CMake function signatures, `Project.json` `version`, `toolsContract`, save
  or settings formats that cannot read the old one, the editor preview API.
- Notes: one `ReleaseNotes/<version>.json` per version, `{"date", "changes", "breaking": [{"change", "migrate"}]}`.
  A test checks the version matches and dates are `YYYY-MM-DD`. The launcher shows every note between two versions.
- Engines download on demand from the tag's source archive, checked against `EngineCopy.json` in its GitHub release. The
  launcher carries one engine for offline first use. An engine release needs no launcher release unless the contract or
  `ToolUi` changes. Switching engine is tried in a temporary copy of the project first.

### D17. Render system
`Painter` records draw calls per frame and sends them to OpenGL 3.3 core through the engine's own `GpuDevice`:
premultiplied alpha, SDF shapes, a dynamic glyph atlas, bloom from emission, RAII state scopes. raylib is only the platform
layer. Owning vertex format, uploads and shaders is what lets quality and speed improve in one place. Usage:
[Painter](../Graphics/Painter.md), [Rendering](../Graphics/Rendering.md).

Deferred: GPU time queries, asynchronous PBO readback for editor previews, and sharing `HandlePool` with `NullDevice`.
Shader clipping and subpixel text positioning wait for measurement; `StaticTexture` and `PixelBuffer` remain unimplemented.

### D18. Console panel
A real terminal in the editor (ConPTY + libvterm, drawn with ImGui; core in `Terminal/`). The editor writes no chat UI of
its own: agent CLIs run as they are, so nothing has to be rewritten when they change. Usage: [Editor panels](../Editor/Usage.md#window).
Word and line selection by double/triple click remain deferred.

### D19. Open source
- Public repositories under the `YADIENGINE` organization: `YADIENGINE/engine` and `YADIENGINE/yadien` (launcher).
- MIT, `Copyright (c) 2026 YADIEN contributors`; `release` adds the engine to a game's `ThirdPartyNotices.txt`.
- Documentation is English.
- No secrets in the repositories; commits use a noreply address; history is never rewritten, because projects pin commits.
- Releases are built locally and uploaded by hand; CI and code signing (SignPath) wait.

### D20. Editor automation through the control endpoint
The open editor is driven and observed through its control pipe (JSON lines) with `python Engine ctl <command>`.
- Generic automation, not an AI feature; no MCP server, since a CLI works with every agent.
- Every state change shows a "Control: ..." notice and is logged; the developer's input is never blocked.
- Screenshots and inspection sync with the worker first and wait for a fresh frame.
- `seek` advances the playback clock in fixed steps, rewinding through a reset.
- The Console's shell gets `ENGINE_EDITOR_CONTROL`, so commands reach that editor when several are open.
- Later: headless rendering without an open editor.

### D21. Agent knowledge and the editor's agent workspace
- `AgentDocs/Agent.md` is short and loaded every session; `AgentDocs/Skills/<name>/SKILL.md` are procedures; recipes, API
  pages and `Docs/` are reference. `check` validates skills and every `python Engine ...` call in AgentDocs.
- `ctl find` / `describe` turn a subject into a scene and one `ctl open` command; `ctl note` reports progress.
- The editor knows agent CLIs by name and runs them in Console sessions. It never calls a model or stores keys.
  "Agent control" can make the endpoint read-only; "Visual checks" refuses captures so looking stays with the developer.
- Deferred: a Claude Code plugin from `Engine/`, and an MCP server (decide by measurement, not by default).

### D22. Asset library
Ready-made graphics and audio are browsed in an Assets panel sharing the scene viewport. `Engine/Assets/Graphics` is
read-only, `Game/Assets/Graphics` is free-form. Every graphic is a set. Registration compiles into the editor worker only,
so Release has no catalog. Prefabs and baking are deferred. Usage: [Asset library](../AssetsAndPlatforms.md#asset-library),
[Graphic assets](../Editor/Api.md#graphic-assets).

Moves rewrite literal includes; renaming remains a code refactor. C++20 modules were rejected because improving this rare
operation did not justify weaker IDE support, a newer CMake requirement and a second source convention.
If prefabs return, keep logic outside asset folders for headless use; no entity/component system or drag-and-drop
composition, and the editor generates skeletons only.

### D23. Tool look: Cabinet
Launcher and editor share one look in `ToolUi/`: warm graphite surfaces, regions separated by tone rather than lines, one
corner radius (3 px), one control height (30 px), at most one amber button per screen, IBM Plex Sans and Mono, and the
logo's 5x7 pixel block (`ToolPixel`) as identity.
- Rejected: gradients, glass, blur, glow, shadowed card grids, pill buttons, large radii, emoji, greetings. Motion only
  shows running work. Nothing game-specific.
- The editor adds a periwinkle used only by agents. Its buttons carry meaning by colour (`Editor/Ui/Style/EditorTheme`):
  green starts and marks live, a rose tint (never a fill) stops, teal is work in progress, amber is selection, pause,
  warnings and the Inspector's primary action. Only a button that starts something is filled.

### D24. Prebuilt engine
Each engine release ships Release static libraries and the editor executable. A project links them and compiles only its
own code; sources stay in `Engine/` for the compiler's headers and for readers. No Debug libraries: Debug compiles the
project unoptimized against Release libraries. `"engine": {"build": "source"}` restores the full source build, and a
project falls back to it when the package cannot be right (a checkout, an edited copy, an older compiler, its own
runtime, tests). Live tuning and development checks are decided per executable, so both builds keep them. Usage:
[Prebuilt engine](../Prebuilt.md).

### D25. Source control
Git is part of the editor's menu bar plus a file drawer, not a dock panel. It runs the system `git.exe`, so LFS,
credentials, hooks and submodules behave as in a terminal. Every state names a next step in plain words, and nothing it
offers can lose work: no force push, reset or discard; Pull fast-forwards unless Merge is chosen; stashes are named and
kept on conflict; nothing is ticked by default; submodules stay on their branch. Rejected for now: history graphs, rebase,
a three-way merge editor.

### D26. Deterministic simulation, replay and scenario tests
Usage: [Simulation](../Simulation.md).

Gameplay advances in fixed steps from a seed and per-step input, so a run can be replayed, checked by hash and driven
without a window. The guarantee is the same build on the same machine. Online play is out of scope.

**Rules:**
- Simulation code builds with an explicit `/fp:precise`; `/fp:fast` changes results.
- State is hashed field by field (FNV-1a 64), never as raw struct bytes: padding differs between runs.
- No global random generator. A simulation owns its generators, seeded from the run's seed; visual code owns separate ones.
- Hitstop and slow motion stay in `TimeScale`, outside the simulation: they change steps per real second, never what a step
  does. Steps, not seconds, are the exact position in a run.
- An input latch delivers a press to exactly one step; held state follows the latest frame.
- Tuning belongs to the build, not the tape; a tape stores a digest of tuning so `sim verify` can report `tuningChanged`.

**Decided:**
1. *A plain `Simulation` class* in a low-level target that cannot reach drawing or audio, hosted by a scene through
   `SimulationRunner`. `Game` needs a window, so headless code never goes through it. One `State(StateVisitor&)` feeds the
   hash, the dump and scenario checks.
2. *The tape stores per-player actions,* not raw device input: named buttons (at most 32 per player) with `held` and
   `pressed` kept apart, axes rounded to a fixed precision, and a command channel. `InputBindings` maps keys to actions
   before the tape.
3. *Whole steps per second, 1 to 240, default 60,* fixed for a run. `ctl step N` and `ctl seek S` keep their seconds
   meaning everywhere; exact steps use `--sim`, `--step`, `--steps`.
4. *One log per scene:* the tape in a simulation scene. Rewinding replays the tape from step 0 with current parameters;
   input during that replay cuts the tape there. No snapshots yet.
5. *`RegisterSimulations` and `engine_add_simulation`* build a static library for game, worker and tests plus a console
   tool for `sim list | run | verify | diverge | state`. One JSON object per answer; exit 1 is a failed check, 2 is usage.
6. *Tapes are text JSON* with sparse changes. A different id, step rate or schema refuses to play; different versions are
   reported and hashes decide.

### D27. Collision
Usage: [Collision](../Collision.md).

Collision detection, not a physics engine: `Core/Physics/Collision` is a small API in engine types over Box2D, vendored
unmodified; Box2D's names reach no public header. `EnginePhysics` depends only on `EngineSimulation` and builds with
`/fp:precise`. Dynamics may later sit in `Core/Physics/Dynamics`.

1. *The world is an index.* The game owns positions and calls `Move`; positions in the world would split game state and
   hide it from `State`.
2. *`Pairs`, not contact events,* so the world keeps no state of its own between steps.
3. *Results do not depend on the tree's shape:* id order, ties to the lower id.
4. *Errors throw in every configuration;* Box2D's own checks exist only in its Debug build.

Deferred: sliding along walls, contact events, tile grids, rigid bodies, an engine grid (the tree was measured good enough).

### D28. Animation
Usage: [Animation](../Animation.md).

Own code in `Core/Animation`, headers only, depending on `Utilities`. Constant data: `Track`, `Clip`, `ClipSet`,
`Timeline`. A player: `Animator`. A value that moves by itself: `Tween`, with `UpdateAll` and `TweenSet`. Each comes on
steps and on seconds. `Graphics` draws; `Animation` computes values over time. Libraries (Tweeny, Choreograph) were
rejected: no named moments, callbacks a hash cannot read, stored positions.

1. *Steps inside a simulation:* `StepTimeline` has `int` positions; `SecondsTimeline` is for views; one template.
2. *Moments cross as `before <= at < after`,* so one call per step meets every moment once, including 0.
3. *Names, not ids;* unknown names throw in every configuration.
4. *The animator is driven by code, not a state graph.* A graph needs a visual editor and duplicates the rules; one could
   later sit on top of `Play`. `Play` of the current clip does nothing.
5. *A tween is the value, not a pointer to one.* Pointer-based tweening (`To(enemy.scale, ...)`) dangles when values move
   and cannot go in `State`.
6. *A pose is a typed struct that lists its fields once* (`Fields`), so the compiler checks tracks.
7. *An animator's state is clips and clocks;* only an interrupted fade stores a frozen pose.
8. *A tween chain is four inline parts;* tweens stay trivially copyable. A curve is a `Curve` value, not a function pointer.
9. *`Simulation::Moments` is for tools only* (`ctl seek --moment`).

Deferred: state graph, layers and additive blending, custom Bezier curves, clips from text, editor support.

### D29. Effects: cues, one shape, and what Animation is for here
Usage: [Effects](../Graphics/Effects.md).

Names belong to cues, not effects. An `FxCue` is constant data: typed actions with times. `FxCueSet` names them and
`FxStage::Play` plays one. Single effects stay typed methods.

1. *No `fx.Play("shake", ...)`:* a name-based door needs an untyped bag of parameters and fails late.
2. *Actions are a closed `std::variant`,* so a cue can be listed, validated and later loaded from text. A sound is not an
   action: Fx must not depend on Audio.
3. *Waiting actions are copied into the stage;* delays run on `Update` time, so a hitstop holds them.
4. *`FxStage` gets a `TimeScale*`;* a cue that changes time throws without `SetTime`.
5. *Tweens only for single moving values.* Rings, shockwaves and text hold an age and a `Curve`; shake, punch and pulse
   retrigger by "the larger wins".
6. *`CameraShot` is a pose.*
7. *`ParticleRecipes` is data;* a game's own burst is the same kind of thing.
8. *Event effects take a style per event;* continuous devices take it at construction. Invalid values throw.
9. *Not effects:* `TimeScale` is in `Runtime/`, `TransitionClock` in `Scene/`, `CollectFlights` in `Gameplay/`.
10. *Particles get a fixed default seed* on `Rng`, so `ctl frames` is repeatable.
11. *Folders by what an effect acts on:* `Particles/`, `Camera/`, `World/`, `Screen/`.

### D30. Effects: two clocks, shake as an amount, cues with a size
Usage: [Effects](../Graphics/Effects.md).

1. *`UpdateReal(realSeconds)` and `Update(gameSeconds)`,* one call per clock from the scene's two hooks (D54), since a
   scene's `Update` does not run while game time stands still. Slow motion's look and toasts run on real time; a flash
   keeps game time so a hitstop holds a hit together.
2. *Trauma instead of seconds:* a shake is an amount that adds up to 1 and shows squared, with smooth noise.
3. *Styles are checked when a stage is made* (`IsValid` per style).
4. *`intensity` scales sizes, not times.*
5. *Cooldowns belong to a name in a set,* on game time; `Play` returns whether it played.
6. *`WorldCamera(paint, base)`* lets a scrolling camera use shake and punch.
7. *Full particle pools replace the oldest.*
8. *Flashes are always rationed:* at most three starts a second (photosensitivity). A flash asked too soon is dropped,
   not delayed. With flashes off, shockwaves and banner curtains go too.
9. *`std::visit` with an overload set,* so a new `FxAction` without a case does not compile.

### D31. Game UI
Usage: [UI](../Ui.md).

A small kernel that knows no widget; everything else uses its public API. A screen is code describing what is on screen
each frame; a tree is built, laid out and drawn, and next frame's input is judged against what was drawn. Behaviour and
looks are separate, and `check` keeps them apart. All UI names start with `Ui`.

1. *Described every frame, not kept objects:* nothing to keep in step with game state, no stored callbacks or pointers
   to values it does not own (as D28).
2. *A real layout pass;* answers come from last frame's rectangles. Nodes are a flat list, parents first; widths before
   heights.
3. *Direction keys find what lies that way on screen;* sideways distance counts three times.
4. *The kernel knows no widget.* If a widget needs a kernel change, the kernel lacks a general idea; decide it here.
5. *Each widget's look is a type, found by type;* default values draw it plainly.
6. *Plain by default, neon as a choice.* `Theme` holds no widgets.
7. *A cursor per player.* The mouse acts as the first player.
8. *Keyboard and mouse only for now;* moves go through `UiKeys`, so a gamepad would be one more source.
9. *Layers carry input, focus and closing;* a closing layer is still described without input.
10. *Sounds are events,* so behaviour does not depend on audio.
11. *No separate CMake target;* `check` enforces the layers.
12. *No visual editor now;* if one becomes a near goal, reopen decision 1.
13. *An element can capture direction keys* (`UiTraits::capture`, `Step()`).
14. *Tabs are one cursor stop;* each tab is mouse-only.
15. *Elements keep two rectangles:* unclipped for the cursor, clipped for the mouse.
16. *A list scrolls to follow keys, not the mouse.*
17. *The kit works on plain values* (`UiOptionsValues`); `Game::UiOptions` / `ApplyUiOptions` connect them.
18. *Sounds play automatically through `UiListener`,* told at each `Ui::End`; `UiSoundListener` is the one `Game` sets.
19. *A widget reports a change; the kernel does not guess.* `UiElement::ReportChange` adds a `Changed` event, called once
    from `if (value != before)`. A game's value widget must call it; forgetting it is silent.
20. *`Back` is an event only when a screen answered it* (`Ui::Back`, not `const`).
21. *`UiSound` is a meaning the kernel carries and does not read,* which keeps `SoundKey` out of kernel and widgets.
22. *A control's press has no sound of its own;* only `ReportChange` speaks.
23. *`UiSounds` lives among a `Ui`'s looks;* `GameSettings::uiSounds` wins. Assigning a whole look set drops it; use `Set`.
24. *"Never over itself" is the listener's rule* for cursor moves and changes.
25. *`back` plays once a frame;* nothing else is limited.
26. *Sound tables are checked in every configuration* at the earliest point: `Game`'s constructor and each `End`.

Later, when a game asks: tables, large lists, rich text, dragging, text entry, gamepad, `ctl ui`.

### D32. Sound tables
Usage: [Audio](../Audio.md#sound-tables).

A game's sound effects are a `constexpr` table with one row per value of its own enum; `game.Sounds().Play(Sfx::Hit)`
plays one. Tables generate ordinary catalog entries.

1. *An enum, not strings,* so a typo cannot wait for run time.
2. *No hand-numbered ids;* `RowsInIdOrder` makes a value without a row a compile error.
3. *`SoundRow`'s constructor throws,* so the same check is a compile error in `constexpr` and an exception at run time.
4. *The row count is in the type* (`Size == Enum::Count`).
5. *At most 8 files per row* (`std::array<std::string_view, 8>`).
6. *`SoundOverlap` is `Copies(n)` or `Skip`;* `Copies` counts per file.
7. *`SoundPlayer` lives in `EngineHeadless` behind `SoundOutput`,* so its logic is tested with a fake.
8. *Cooldowns read unscaled frame time.*
9. *Generated ids start at 2^32.*
10. *The editor's `SceneServices::sounds` is the worker's `Game::Sounds()`.*

Open hole: a `SoundKey` of an enum whose table was never registered compiles and throws when played; `Has` lets a game
check at startup. Left out: mixer and ducking beyond D39's buses, music in tables, folder scanning, sounds in `FxCue`,
default sound files.

### D33. Inspector and tuning
Usage: [Tuning](../Tuning.md#editor), [Editor usage](../Editor/Usage.md).

One Inspector edits both a scene's controls and the tuning files the running scene reads; there is no Tuning panel.
Every value says what it is, so a temporary change is never mistaken for a lasting one.

1. *It shows the selected scene* (controls, then one section per tuning file read), or one tuning file on its own.
2. *`Tuning<T>()` marks its file* in the current use period; the worker reports `tuning-used`.
3. *One model and widget set:* `PropertySpec` / `PropertyValue`, drawn by `PropertyWidget`.
4. *The file is the only lasting write path.* A drag previews in worker memory; its end writes the file once.
5. *Modified means "since the last commit"* (`git show HEAD:`).
6. *Undo covers the editor's own writes,* only while the file still holds them.
7. *`note`, `unit`, `group` live in the file.*
8. *`"type": "choice"` generates an `enum class`.*
9. *Lengths change through `ResizeTuningField`;* the scene then needs a build.
10. *Paths are relative to the project folder.*
11. *`TuningFiles.json` (version 2) names roots,* so new files show before a build.

### D34. Simulation events
Usage: [Simulation](../Simulation.md#events).

A simulation emits typed events into a list it owns (`SimulationEvents<E>`); each view reads through its own cursor
(`SimulationEventReader<E>`). Events are output, not state, and are not hashed.

1. *One list per game, a `std::variant` of plain structs,* so order across types is kept. Each type has a `Name` and an
   optional `State(StateVisitor&)`.
2. *The game owns the list;* `Simulation` only exposes a type-free `EventLog()` for tools.
3. *`Emit(step, event)` stamps the step.*
4. *Readers keep cursors;* the list keeps one frame's worth of steps plus one. A reader that falls behind counts `Missed()`.
5. *Catching up is silent by default* (`EventsSilencedUntil`), so a seek does not replay every sound.
6. *Tools see events:* `expectEvent`, `untilEvent`, `sim run --events`.
7. *`Utilities/Overloaded.h`* builds visitors, so a missing case does not compile.

Left out: data mappings from events to cues, per-type queues, listeners inside the simulation.

### D35. Game camera
Usage: [Camera](../Camera.md).

`Core/Camera`: `Camera2D` and `Cameras` (main, split screen). It computes and draws nothing, builds with precise floats
and links below `EngineHeadless` (`EngineCamera`).

1. *`Camera2D` is the one class a user meets;* rules are pure functions in `Camera/Internals/CameraSolver`.
2. *A camera may be part of the rules:* simulations may include `Core/Camera` (held to Utilities and Animation; its
   `Debug/` is forbidden to them). `Update(step.seconds)` and `State` in a simulation, `View(alpha)` to draw.
3. *`CameraView` lives in `Core/Camera`* and carries a `viewport`.
4. *`Painter::Camera(view)`* clips and transforms in one scope; Graphics depends on the view, not the reverse.
5. *Critically damped smoothing* (`Animation/SmoothDamp.h`); `SpringPoint` stays for UI.
6. *Dead zone and look-ahead are fractions of the view.*
7. *Several targets:* weights move the middle; `zoom.fit` fits every target between `min` and `max`.
8. *`Cameras::Split(1..4, layout, gap)`;* new cameras copy the main one.
9. *`FxStage::WorldCamera(paint, view, limits)`* keeps effects in the view's place.
10. *No camera stack or priorities;* a tween of `CameraPose` covers cutscenes.

Left out: per-view effects in a split, dynamic split, platformer zones, rotation-aware limits, a camera panel.

### D36. Graphics own their look
Usage: [Graphic assets](../Editor/Api.md#graphic-assets).

A ready-made graphic lives in `Assets/Graphics` and draws as intended with no look given; callers override only what
they need. `.asset.cpp` keeps registration and passes no colours. `Core/Graphics` keeps primitives, helpers and what Core
systems call; Core never depends on Assets.

1. *One signature:* `DrawX(paint, ..., const XLook& look = {})`; per-item sets use `std::optional` fields.
2. *`Painter::Opacity` fades* without choosing colours.
3. *Themed graphics stay themed* (backdrops, UI widgets).
4. *`EngineAssets` links above `EngineHeadless`;* its include root is `Assets/` (`Graphics/DefaultIcons.h`).
5. *Registration stays apart,* since prebuilt libraries cannot `#ifdef` the catalog out.
6. *Folders are free;* a graphic may sit directly under `Graphics/`.

### D37. Unused sounds stay out of the package
Usage: [Release](../Release.md#only-the-sounds-a-game-uses), [Audio](../Audio.md#ready-made-sounds).

`release` packages only the sounds the built game names: the game prints its asset manifest when `ENGINE_ASSET_REPORT`
is set. Development builds copy everything. Fonts and other files are not filtered. A sound loaded later by code needs
`keepAssets`, and the release summary names every sound left out. A `package_assets` hook wins; `keepAssets` beside it is
an error. Engine sounds are referenced as `Engine/Audio/...`.

### D38. Lighting
Usage: [Lighting](../Graphics/Lighting.md).

`Graphics/Light/LightLayer` darkens what was drawn so far to ambient plus the lights, which add up in their own image
multiplied over the target. View code; it decides nothing.

1. *In `Graphics`, not its own folder;* `check` keeps it from reaching further and `Paint` from reaching it.
2. *Lights are data described every frame;* no list to clear, no handles.
3. *`Painter::Light`, not `Glow`:* no quality setting removes a light. (`LightStyle` became `RadialGlowStyle`.)
4. *One primitive for lit and shadowed:* a polygon of what the light reaches, falloff per pixel in the shader.
5. *`Blend::Multiply` changes colour only,* never the bloom source.
6. *Emissive drawing goes after the layer.*
7. *Half resolution by default.*
8. *No quality setting, normal maps, or brightening past a pixel's colour* (that needs a float canvas).
9. *A layer inside a camera scope is an error* (`Painter::CurrentTransform`).
10. *Colliders do not stop light by themselves:* a hitbox is not a shadow shape, and a collider flag would change the
    replay hash.
11. *A one-way bridge in one file:* `AddOccluders` reads a `CollisionWorld`; only `CollisionOccluders` may see `Physics`.
12. *Moving objects give the outline the view draws;* the bridge is for what stands still.
13. *Rays, not a sorted sweep,* to survive touching, crossing and aligned edges.
14. *The outline stops at a box past the radius.*
15. *Near side lit by default;* a light inside a closed shape ignores it.
16. *`VisibilityPolygon` lives in `Utilities`* and knows no light. Rules ask `CollisionWorld::Raycast`.

GL or shader changes are checked with `EngineGpuPixelCheck`, which reads real pixels; `NullDevice` tests cannot see them.

### D39. Spatial and layered audio
Usage: [Audio](../Audio.md#sounds-in-the-world).

Sound is miniaudio's engine (vendored, only in `Core/Audio/Internals`), with the engine's own shaper on every voice, bus
and the master. World sounds, live sounds and ambience layers all start from sound table rows.

1. *miniaudio,* not a mixer of our own or another library: clean loop seams, streaming off the main thread, permissive
   licence, one file.
2. *`AudioShaper` on every stop:* per-ear gain, fading volume, low-pass; tested on plain samples. miniaudio's pan is not
   used.
3. *Files keep their own rate;* converting at load put zeros at every loop.
4. *Constant power* for pan and crossfades; the master keeps 1/√2 headroom.
5. *A state variable filter (Simper) for muffling;* it is out of the path at 0.
6. *A view's sound, never a simulation's;* `check` keeps `Audio/` out of simulations.
7. *A handle owns its sound:* dropping a `LiveSound` fades it; stale handles do nothing.
8. *The listener is plain data;* several listeners serve split screen.
9. *Distance through a row's `SoundFalloff`* (full, a `Curve`, silent).
10. *Three buses:* World, Ui, Music. World live sounds pause with game time.
11. *Loops and layers are rows,* so `release` keeps their files (D37).
12. *Voices stopped part-way are rebuilt* before reuse.
13. *`AudioOutput::Offline`* runs the real mix for tests; `ctl audio capture` records device output.

Left out: HRTF, reverb, music sequencing, a mixing editor, a limiter, Doppler, collider occlusion.

### D40. Light polish: flicker, short flashes, soft shadows
Usage: [Lighting](../Graphics/Lighting.md).

1. *`Flicker` is a function in `Animation` with `flashes` a required argument,* so the player's choice cannot be forgotten
   and `LightSource` stays plain data. Without flashes it wanders slowly instead of cutting out.
2. *Flashes connect in one line: `.flashes = _fx.LightFlashes()`.* `Light` and `Fx` do not include each other.
3. *A flash with no radius raises the ambient* and no occluder stops it; with a radius it is an ordinary light.
4. *Flash clocks and scale follow `FxFlash`.*
5. *One shared `FlashLimit`* for screen flashes, light flashes and bolts.
6. *Turning flashes off ends one already showing.*
7. *Soft shadows blur the light image* (`LightLayerOptions::softness`, world units scaled by zoom, 1 to 24 image units),
   only in frames where a shadow falls. Chosen over per-light samples, soft wedges and distance fields. Costs: the same
   spread near and far, and a thin wall leaks that much. `Painter::BlurredImage` is public.
8. *A wide spread passes twice* to avoid bands.

A penumbra that grows with distance can come later behind the same option.

### D41. Analog screen finish
Usage: [Analog finish](../Graphics/Painter.md#analog-finish).

Grain, colour fade, tint, wobble and tape are fields of `PostSettings`, computed in the existing compose pass.

1. *Fields, not a system:* `paint.Post() = Mix(calm, disturbed, danger)`; the game owns the number.
2. *`Mix(PostSettings, PostSettings, t)`* joins Animation; `t` is clamped.
3. *An effect that is off has no say* in a mix; strengths blend linearly.
4. *Rhythms keep a phase* in `PostChain`, so a speed change does not jump. Rewinding does not restore it.
5. *Noise is an integer hash in the shader* (pcg3d) at 24 frames per second, from a `double` clock.
6. *`RenderQuality::screenMotion` is the player's switch;* it stops what moves and keeps what holds still.
7. *Readable by construction:* no field has a non-zero default; wobble fades over the outer 22%.
8. *Compose order:* wobble and line shifts, then colour, aberration, bloom, fade, tint, dashes, scanlines, vignette, grain.
9. *Not `ScreenGlitch`,* which stays a timed `Fx` burst.

Out of scope: game shaders, CRT curvature, palette reduction, a low-resolution mode.

### D42. Atmosphere and weather
Usage: [Atmosphere](../Graphics/Atmosphere.md).

`Weather` is a plain pose in `Core/Weather`; `Graphics/Atmosphere` shows it. Lightning is a cue. There is no weather system.

1. *Data and drawing apart:* simulations may keep `Weather` (held to Utilities and Animation).
2. *No weather class:* a change is `SecondsTween<Weather>`, a sequence a clip.
3. *One `DriftField` for everything that drifts,* taking a look; a game's own kind uses only public API.
4. *A repeating world tile, not spawning,* so no view finds an edge.
5. *The amount is a count* of pieces in seeded order.
6. *Fog is a `Painter::Mesh` of value over the view,* not a texture, `Glow` or shader.
7. *Behind and in front are two calls.*
8. *`parallax`:* 1 stands in the world, 0 on the screen (`CameraView::Parallaxed`).
9. *`TopDownAtmosphere` and `SideViewAtmosphere` are data.*
10. *Lightning is a cue* (`FxBolt`, `FlashLights`); `Fx` and `Graphics/Atmosphere` do not include each other.
11. *Air is view code:* its own `Rng` and clock, never in `State`.
12. *`backgroundEffects` halves pieces and coarsens fog;* a cost ceiling holds for wide views.

Out of scope: side-view splashes, settling snow, wet floors, gusts, lamp-scattered fog, a forecast.

### D43. Several agents in one editor
Usage: [Several agents](../Editor/Automation.md#several-agents).

Every control request names its Console session (`ENGINE_EDITOR_SESSION`), and one session at a time may change or capture
the editor (`ControlLease`). The pipe serves 8 clients. `ConsoleSessions` owns the terminals apart from any panel.

1. *A hold, not a queue:* a refused agent hears it at once. The hold lasts while answering and 60 s after.
2. *Only changes and captures need it;* `status`, `inspect`, `logs` and notes never wait.
3. *A lost hold is said once.*
4. *The developer always wins;* UI changes end the hold.
5. *Clients outside the Console share one identity.*
6. *A closed session frees its hold.*
7. *One request per connection, one held command at a time.*
8. *Events and hook tool lines are kept per session.*

Out of scope: windowless rendering for private checks, identities outside the Console.

### D44. The Agents drawer
Usage: [Agent workspace](../Editor/Usage.md#agent-workspace).

Agents live in a drawer under the toolbar's agent strip, not a dock panel: a chip per agent session and the chosen one's
terminal or activity. The strip shows a dot per session.

1. *A drawer at the top right,* about a quarter of the workspace (capped at 45% × 65%), over the panels, so the layout never
   changes. Esc closes it only when its terminal lacks the keyboard.
2. *The terminal is in it;* a terminal is drawn in one place per frame (`ConsoleViews`).
3. *Chips keep the Console's order;* a waiting one glows. Tabs: *Terminal | Activity*.
4. *Agent requests open here;* shells stay in the Console.
5. *The strip speaks of the waiting session first.*
6. *One chip for every agent outside the Console.*
7. *Keys:* Ctrl+Shift+A toggles, Ctrl+PageUp/PageDown and Ctrl+1…9 choose, Ctrl+Shift+T switches tab, Ctrl+Shift+S sends
   the scene, Ctrl+Shift+N starts one. Alt+letter and `[` `]` are not used.

### D45. Polygon geometry: earcut and Clipper2
Usage: [Painter](../Graphics/Painter.md#holes-and-fill-rules).

Paint triangulates with earcut.hpp and resolves areas with Clipper2, both vendored and private to `EngineHeadless`.

1. *earcut triangulates, Clipper2 decides areas;* Clipper2's own triangulation is beta and refuses intersections.
2. *Triangles come from the real outline;* only coloured corners move half a fringe inward, so slivers fold within a pixel.
3. *`FillRule` lives on `Path`, `NonZero` by default.*
4. *A filled path's border and glow follow the resolved area.*
5. *`Polygon` stays the fast path;* Clipper2 runs only for paths, glows and aligned borders (grid 1/256 device pixel).
6. *Glow is Clipper2's round offset minus the shape.*
7. *`StrokeAlign::Inside` / `Outside` work for polygons and paths.*
8. *No public shape operations yet;* they would need a simulation-linkable target with `/fp:precise`.

### D46. Smooth noise: FastNoiseLite
Usage: [Rendering](../Graphics/Rendering.md#geometry-and-timing).

FastNoiseLite behind `Utilities/Random/NoiseField.h`: `NoiseSettings` and plain functions `Noise(settings, point)`,
`Noise(settings, x)`, `Warp(settings, amplitude, point)`. `Hash`, `Random01` and `NoiseHash` stay in `Noise.h`.

1. *Plain functions of plain settings,* no object; settings can sit in tuning files.
2. *`frequency` means the same for every kind:* at 1, about as often as values one unit apart.
3. *`Warp`'s amplitude is the largest push of one layer.*
4. *Its own target, `EngineNoise` (now `EngineRandom`, D49), with `/fp:precise`,* linked by `EngineSimulation`.
5. *Simplex by default.*

Out of scope: cellular options, 3D noise, noise in a simulation (needs a `StateHash` test when it comes).

### D47. Plain-struct reflection: PFR
Usage: [Simulation](../Simulation.md#state), [Animation](../Animation.md#clips-and-poses).

Poses and simulation state are plain structs whose fields the engine reads by itself, with PFR (standalone, header only)
behind `Utilities/Reflection.h`. No field lists: `PoseFields`, `PoseField` and `Fields()` are gone; `PoseFields.h` became
`Pose.h`.

1. *A forgotten field is a bug class worth a dependency:* it dropped silently out of blending and the replay hash.
2. *One way to make a pose:* any plain struct of animatable fields; something that should not animate stays outside it.
3. *PFR is the one third-party library a public header includes,* because templates walk a game's own structs. Only
   `Reflection.h` names it; `check` refuses `pfr` includes elsewhere, games included.
4. *Names are the struct's own;* dumps and hashes of engine poses did not change (same names, same order).
5. *`MixValue` blends any plain struct field by field;* a type's own `Mix` wins. `Mix(Weather)` and `Mix(CameraShot)`
   went, and `SecondsTween<CameraPose>` now compiles.
6. *`StateVisitor` takes the whole state:* `Fields(struct)` at this level, `Field(name, struct)` as a group, plus lists,
   `std::optional`, `StepSmoothed` (current value) and anything with `State(visitor)`. Pointers are refused.
7. *An unnamed value is its group:* list items dump as `name[2]`, not `name[2].`.
8. *Events without a `State` show their fields.*
9. *Reflected types need external linkage:* MSVC reads names through an extern object, so a struct in a function or an
   anonymous namespace stops the build. Documented, not worked around.

Left out: a `Fields` macro of our own (repeats every name), private-member reflection, C++26 reflection (PFR moves to it).

### D48. Text shaping: kb_text_shape
Usage: [Text](../Graphics/Text.md#shaping).

`FontFace` shapes text with kb_text_shape (one C header, zlib) before laying it out; stb_truetype stays for rasterizing
and metrics. kb lives in `EngineTextShape`, private to `EngineHeadless`; only `Graphics/Text/Internals` includes it.

1. *Shape once per layout, at no size:* lengths are in units of the font's height, so `TextFit::Shrink` and every size
   reuse one shaping.
2. *Lines break at Unicode break opportunities, greedily, between graphemes for an over-long word;* spaces hang, as
   before. Latin and Turkish layouts are unchanged except for the font's own ligatures.
3. *Bidirectional order per line:* kb's run levels, reordered by rule L2; each run's glyphs come in drawn order.
4. *Fallback fonts belong to the face:* `FontFace::AddFallback` (also `GameSettings::fontFallbacks`) owns them, so a
   block can never outlive the font of a glyph. First font that has a character wins; lines keep the face's metrics.
5. *A fallback is drawn with the face's em,* not its height: script fonts have tall ascents and would look small.
6. *Letter spacing turns optional ligatures off.*
7. *A character no font has is the face's `?`,* as before; zero-width ones vanish.
8. *`TextBlock::Glyph` grew `y` and `font`;* `FontData::Kerning` went, kerning comes from shaping.
9. *Honest limits:* simplified bidi, no Thai dictionary, horizontal only, no automatic right alignment.

Left out: carets and selection (a cluster index per glyph), text direction in `TextStyle`, script fonts in the starter.

### D49. Procedural kit
Usage: [Procedural](../Procedural.md).

Everything random lives in `Utilities/Random` (`Rng`, `WeightedPick`, `DrawTable`, `Scatter`, and `Noise.h` /
`NoiseField.h`, moved from `Utilities/Math`), built as `EngineRandom` (was `EngineNoise`). Generated layouts live in
`Core/Procedural` (`EngineProcedural`): `GenerateRooms`, `GenerateRandomWalk`, `GenerateCaves` and `Regions.h`, over
`Grid<T>` (`Utilities/Geometry/Grid.h`) of `MapCell`. No library: each piece is under 150 lines, and each had to take
our `Rng` and a fixed order.

1. *Group by what it gives, not by what it uses:* randomness is a foundation every layer includes, so it stays in
   `Utilities`; a cave generator uses `Rng` the way particles do and is a system of its own.
2. *Both targets build with `/fp:precise` and below `EngineHeadless`;* `engine_add_simulation` links `EngineProcedural`.
   `check` lets `Core/Procedural` include only `Utilities` and itself.
3. *The grid belongs to neither generators nor tiles:* `Grid<T>` is plain data in `Utilities`; a tile kit will use the
   same type and own what cells mean. `StateVisitor` writes a grid as `size` and `cells`.
4. *A table is fixed data, its memory a plain struct:* `DrawMemory` goes into the state as it is, so pity survives a
   replay. Pity is "after N misses"; the most overdue entry wins without a roll.
5. *Generators keep the border wall and return one connected floor area;* caves fill small areas and dig shortest
   tunnels to the rest (`ConnectRegions`).
6. *Poisson sampling draws ring offsets by rejection,* so it needs no sine or cosine; points come in the order placed.
7. *Noise in a simulation is proven:* a toy simulation reads noise, scatters, generates caves and draws loot every step;
   the same seed gives the same hash at all 301 steps and its tape replays (`ProceduralReplayTests`).
8. *No pinned hashes:* tests check sameness for a seed and the layouts' properties (border, one area, room gaps), so a
   better generator is not a broken test.

Left out: loops between rooms, several walkers, wave function collapse, a shuffle bag (`Rng::Shuffle` covers it),
soft pity (rising odds), density masks for scattering, a tile grid kit.

### D50. Particles: streams, life curves, areas
Usage: [Effects](../Graphics/Effects.md#streams).

No library: Effekseer authors effects in its own editor (agents cannot write them) and draws through its own renderer;
the Particle System API, SPARK and Thor are 3D, unmaintained or bound to another framework, and the first uses the
global `rand()`.

1. *A stream's state lives in its owner:* `ParticleEmitter` is a value like a tween (last place, share of a particle
   owed); `FxStage::Emit` runs it. No names or handles in the stage, so a dead object's stream goes with it.
   `FxStage::Trail` and `FxTrail` are gone: a steady stream is not a one-off cue action.
2. *Rates per second and per unit travelled,* with fractions carried over; each frame's particles are spread along the
   way and aged by the rest of the frame. The player's particle share scales the rate, so it thins even one a frame.
3. *Life is fixed data:* up to eight colour keys and a size curve, so a `constexpr` theme holds them (`Track` keeps a
   vector). Keys multiply the emitted colour; `ownColors` uses them as they are. `fadeFraction`, `sizeScale` and
   `endSizeScale` are gone.
4. *Particles share their look:* a particle holds its own values and an index into the system's looks (appearance,
   motion, custom draw); a look with no particles left is reused and drops its drawing. Measured at 5,000 particles
   (Release, `[.fx-perf]`): emit 27 to 9 us, update 20 to 14 us a frame; recording the draws is unchanged at ~330 us
   and dominates.
5. *Areas and inheritance are emission data,* so bursts use them too: Point, Disc, Ring, Box, Line, optionally aimed
   outward. A `ParticleSource` carries the velocity; `FxPlay::velocity` passes it through cues.
6. *Weather that fills the view stays `DriftField`;* emitters are for local sources.

Left out: sprite sheets and frame animation, collision, attractors, GPU particles, custom drawing for streams.

### D51. Project settings
Usage: [Project settings](../ProjectSettings.md).

The editor's first lasting edits outside tuning: title, window, canvas, font and identity move from `GameSettings` code
to `Game/Game.settings.json`, edited under the new **Project** menu.

1. *The schema is the engine's, the file holds values only:* `Core/Runtime/ProjectSettings.schema.json` (tuning syntax,
   `value` = default) generates the committed `ProjectSettings.h`; a project's file lists only what differs, so an
   engine can change a default or a bound without touching projects.
2. *One source:* the fields left `GameSettings` for `GameSettings::project`, without a shim. Hosts (worker, tools, tests)
   may still write them; games do not.
3. *Read at start, not live:* window and canvas cannot change under a running game, so there is no `apply` mode and no
   preview. Development builds reread the file at start; Release compiles it through a generated `.cpp` per game and
   worker target, which runs before `main` and works with a prebuilt engine.
4. *Tuning machinery reused, live side untouched:* the same parser, Inspector widgets, undo and git comparison. `text`
   and `path` exist only in schemas, since tuning values are `constexpr` and patched in place.
5. *The exe carries the identity:* a generated `.rc` gives `GLFW_ICON` and VERSIONINFO; the version also feeds
   `Version::Parse`.
6. *A new application id asks first:* it moves every player's options and saves.

Left out: an apply-live mode, lists in schemas, quality profiles, localization, plugin switches, a single C++
generator (the Python and C++ rules stay twinned by shared cases).

### D52. Input map
Usage: [Input map](../InputMap.md).

`X.input.json` names buttons and axes once and gives each player's default keys; it replaces the hand-kept pair of an
enum and `SimulationSettings::buttons`, which drifted silently when one changed.

1. *Buttons and axes are structure, keys are values:* the lists generate `XInput::Button` / `Axis` and their names
   (`DeclareInput`); keys are read from the file when a scene binds the map in development, compiled in Release.
2. *Key names, not codes:* the header holds names, so simulation code that includes it never reaches `Input/`;
   `InputBindings::BindMap` resolves them. A test keeps the C++ name table in the enum's order.
3. *Its own small format,* not a tuning table: a button has any number of keys per player.
4. *The editor edits keys only:* a click removes one, `+` waits for the next key. Buttons change in the file and need a
   build.

Left out: gamepads, an axis from a key pair, player rebinding (N8), a collision-layer matrix (D27 keeps no global one).

### D53. One scene type
Usage: [Editor API](../Editor/Api.md#registration).

The editor's catalog opens the game's own `Scene`; the separate editor scene type is gone, so a screen is no longer
written twice (once for the game, once for the editor).

1. *The editor hooks are `Scene` virtuals with neutral defaults* (`ApplyParameter`, `ExecuteAction`,
   `CaptureParameters`, `Inspect`): the game never calls them, and one class needs no second interface or cast.
2. *Input stays `HandleInput` + `InputCancelled`:* the host cancels; scenes no longer read `input.cancelled`.
3. *One service source:* `SceneServices` lives in `Core/Scene`; `Game::Services()` fills it, the worker copies it and adds
   its log and simulation host. A scene's constructor takes it plus its own values, so the game and a factory build it
   alike.
4. *`HostedSimulation` and `SimulationHost` live in `Core/Simulation`:* a null host simply runs, so the game and the
   editor use the same member.
5. *The worker hosts through the public lifecycle:* `WorkerScene` drives the scene as `Game::Run` does; editor Pause,
   Step and speed reach both clocks, and the scene's `TimeDomain` and `GameTimePaused` hold. Reset stops and destroys
   the old instance before the factory runs.
6. *The catalog stays in `Editor/Api`:* `SceneDefinition`, the registry and auto-registration, so Release carries none.
7. *Destructors called `ShutdownNoThrow()`* while a scene could hold registered objects; D56 removed both.

Left out: a separate editor interface found by `dynamic_cast`, switching scenes from the editor (still `Game::SetScene`
in a Game Flow), the scene-held `Profiler` pointer and application command (nothing read them).

### D54. Stage
Usage: [Stage](../Graphics/Stage.md).

`Core/Stage`: a `Stage` owns a scene's view side (`Cameras`, `FxStage`, an optional `LightLayer` with still occluders, an
optional `Atmosphere` with its `Weather` tween, the sound listeners) and draws it in one fixed order. The scene says what
is there through a `StageView`; recipes no longer write the passes by hand.

1. *Fixed passes per view:* background, floor, ground effects, atmosphere behind, world, air, atmosphere in front, light,
   glow, world effects; then the overlay and the HUD once. One order means a new pass reaches every scene unchanged.
2. *A `StageView` passed to `Draw`, never kept:* virtual passes with empty defaults, only `DrawWorld` required. Stored
   lambdas break D28, and a list of layer objects is a node tree in disguise.
3. *Optional parts are off by default* (`StageSettings::light`, `atmosphere`); asking for one that is off throws.
4. *Two clocks, two calls:* `UpdateReal(realSeconds)` from the scene's `UpdateReal`, which runs in a hitstop and a pause,
   and `Update(gameSeconds)` from its `Update`, which does not. Each moves only its own clock's parts, so a
   `TimeDomain::Real` scene moves nothing twice. One combined call froze banners and toasts in every hitstop.
5. *Built from `SceneServices` and borrowing them:* the effect choice is reapplied when it changes, the time scale serves
   cues, the sound player gets one listener per view (`listeners` turns that off).
6. *Views from elsewhere:* `SetViews` copies a simulation camera's `View(alpha)` (D35) until the next call; an empty
   span returns to the own cameras. Callable in a constructor, so the first frame is right. Split screen runs the same
   passes per view.
7. *Depth without a scene graph:* `DepthOrder` sorts keys by ground y with a stable order; `Raised` and
   `DrawGroundShadow` give height; `FxStage::Mark` is the ground layer's first effect; parallax is
   `CameraView::Parallaxed` in the background pass.
8. *Lights are described once a frame* with the Painter for its clock; every view shows the same light.
9. *`check` keeps `Stage` above Camera, Fx, Graphics, Weather and the sound player,* and out of simulations.

Left out: the projection core (3D point to screen, wireframe), per-view effects, guides for cameras the stage does not
own, changing the canvas size.

### D55. Pool and handles
Usage: [Simulation](../Simulation.md#things-that-come-and-go), [Collision](../Collision.md#items-in-a-pool).

Things that spawn and die live in `Pool<T>` (`Utilities/Pool.h`, header-only); `Add` gives a `Handle<T>` (slot index +
generation) that finds its item until it is removed. `Physics/Collision/PoolColliders.h` links a pool to its colliders.
D27 stays: the world is an index, the game owns positions.

1. *Handles are typed plain structs:* a `Handle<Enemy>` cannot reach a `Pool<Bullet>`, generation 0 is null, and
   `StateVisitor` writes one as fields wherever a game keeps it.
2. *Spawn order, not swap-and-pop:* removal keeps the others' order and new items go last, so loops, nearest-target ties and
   results read the same in every replay. `RemoveIf` asks every item before any leaves, then closes the gaps in one pass.
3. *No adding or removing during a loop over the pool, enforced:* the loop's iterator marks the pool, and `Add`,
   `Remove` and `RemoveIf` throw `std::logic_error` while a loop (or a `RemoveIf` question) runs, before anything moves.
   Collecting changes for after the loop is what the game needs anyway.
4. *The allocator is state:* `State` writes items with their handles, then each slot's generation and the free list,
   because they decide the handles given later; two pools with equal items but different histories hash differently.
5. *A value type:* copies are independent and keep the handles; `Find`'s pointer is for now, the handle is what is kept.
6. *A slot whose generation would wrap is retired,* so no old handle ever names a new item.
7. *One `PoolColliders` per pool, declared beside the item type, owns the collider contract:* the member holding the
   id, where the item stands, the pool's layers. Compile-time data only (member pointers, a capture-free lambda, the
   mask), so it holds no world, pool or callback (D28). `Add` gives item and collider at once, so owner, layers and pose
   cannot be wrong; `Sync` moves every collider once per step, so a forgotten `Move` is impossible; `RemoveIf` and
   `Remove` take the colliders along. Physics sees Utilities, never the other way.
8. *A hit leads back through layers:* the owner is the handle (`Owner()` / `FromOwner`), query results now carry
   `layers`, and `Of` gives the handle only when they overlap the pool's layers, because two pools may give equal
   handles.
9. *Foreign handles are caught as far as the numbers allow:* a handle this pool never gave throws from `Get`; one from
   another pool of the same type with an equal slot and generation cannot be told apart, as with two lists' indices.

Left out: component storage / ECS (revisit with the swarm and bullet kit, system 6). If ever adopted: EnTT only inside
simulations, no signals, observers or meta, a `StateVisitor` adapter that names component types, a pinned version.
Also left out: a stored callback on removal, and `Clear` (`RemoveIf` removes everything and keeps old handles stale).

### D56. Scenes without an object list
Usage: [Runtime](../Runtime.md#lifecycle), [recipe](../../AgentDocs/Recipes/SceneMembers.md).

`GameObject`, `ObjectList` and `Scene::AddObject` / `RemoveObject` / `ClearObjects` are gone. A `Scene` keeps its
simulation and views as value members and calls them from its own hooks; things that come and go live in a `Pool<T>`
(D55). `Scene` owns its lifecycle directly: Start once, Enter per visit, Update by its clock, Draw, Exit, Stop once.

1. *One lifecycle type:* the only real subclass of `GameObject` was `Scene` itself, registered in its own list. The
   list was a node tree in disguise and did nothing a member call does not.
2. *The guarantees stay:* no re-entrant dispatch, a failing hook leaves a consistent state and is not retried (its
   pairing flag is set first, so Exit and Stop still run), input may be cancelled from an input callback, and
   `Shutdown` finishes every step before rethrowing the first failure.
3. *No destructor in a scene:* nothing can be registered with a scene, so only its own Exit and Stop need it alive, and
   the host runs them: `Game::Run` shuts every scene down before it returns or rethrows; a scene is never started
   before `Run`, so one destroyed earlier (a derived game's constructor throwing) has nothing to undo. `ShutdownNoThrow`
   is gone. Debug builds assert when a started scene dies without `Shutdown`.
4. *`WorkerScene` holds content only between its Start and Stop,* so its Stop always shuts the hosted scene down; `Open`
   throws otherwise. A hosted scene whose activation fails is shut down before it dies.
5. *Input is cancelled once at the worker's shutdown:* the worker no longer forwards its own cancellation then, because
   the hosted scene's shutdown cancels it.
6. *`Enabled` / `SetEnabled` are gone:* no host or scene used them.
7. *`Core/Progress` joined `Core/Gameplay`;* `Docs/Architecture.md` places every `Core` folder in a layer.

Left out: a view-side selectable object for the editor (pick, inspect, move a thing in the viewport), to be designed
when a real need appears.
