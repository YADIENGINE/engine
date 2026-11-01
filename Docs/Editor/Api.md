# General editor API

This document is the project-independent contract for editor scenes. Initial setup is in [GettingStarted](GettingStarted.md); using
production drawing and designing content are in [CreatingScenes](CreatingScenes.md).

## Registration

Public headers are under `Engine/Editor/Api/`; they contain no ImGui, Windows handles, or IPC. The project registers
explicitly with the registry it receives:

```cpp
void RegisterProjectScenes(SceneRegistry& registry) {
    SceneDefinition definition;
    definition.id = "project.my-scene";
    definition.name = "My scene";
    definition.category = "Drawing";
    definition.parameters = {{"radius", "Radius", SceneParameterKind::Float, 120.0, 8.0, 300.0}};
    definition.actions = {{"emit", "Emit"}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<MyScene>(services, parameters);
    };
    registry.Add(std::move(definition));
}
```

`MyScene` derives from `EditorScene` (a `GameObject`). Drawing without a model needs only `Draw(Painter&) const`;
override `Start`, `Update(float)`, `Stop`, `HandleInput`, `ApplyParameter`, `ExecuteAction`, `CaptureParameters`, and
`Inspect` as needed. Do not write a separate frame loop; content owns the model and real View, without a second copy of
drawing for the editor.

Rules:

- Registration adds only metadata / factories; it creates no window, assets, or instances. IDs are unique.
- The factory is called on the engine thread when GL, text, and audio are ready; initial values are supplied as a
  complete map, without relying on `ApplyParameter` during setup.
- `SceneServices` supplies canvas size, audio reference, game font (`font`; for measurement outside drawing, e.g. hit
  testing; null if absent), and log/application-command callbacks. References outlive the instance; do not retain them
  after content stops.
- `Update` receives scaled seconds. Visual time is `paint.Time()`: `WorkerScene::Seconds()` resets to 0 on every Reset
  and supplies presentation time through `WorkerGame::DrawSeconds()`; content does not wire up a clock.
- `Stop` and destructors do not throw. Factory / hook failures may terminate the worker; the editor shows the error and
  exit code.

`SceneDefinition::benchmark` is an optional, repeatable workload contract. It carries stable identity/version, the
names of two variants, and parameter differences. Without a definition, the scene supports only live measurement.
`SceneServices::profiler` is a borrowed module reference for recording custom CPU scopes/counters; use it only through
`Modules/Profiler/Profiler.h`. [Profiler guide](Profiler.md).

## Graphic assets

A file named `X.asset.cpp` under `Assets/Graphics` registers a graphic for the Assets panel. Only the editor worker
compiles these files, so games carry no catalog. Keep the drawing itself in ordinary game code and call it here.

```cpp
#include "Editor/Api/GraphicAsset.h"

namespace {
GraphicAsset MakeGoblin() {
    GraphicAsset asset;
    asset.usage = "DrawGoblin(paint, center, size);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t, float time) { /* fit to box */ };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeGoblin);
} // namespace
```

Fill `items` for a set: each item gets its own tile and catalog entry, and `{item}` in `usage` becomes its name. Ids are
`asset:Graphics/Folder/Name[#Item]`; `ctl find` and `ctl open` reach them like scenes. `python Engine asset new NAME`
writes a skeleton. Design: [Asset library](../Design/AssetLibrary.md).

## Parameter and action contract

`SceneValue`: `int64_t`, `double`, `bool`, normalized `SceneColor`, or option text (`std::string`).
`SceneParameter` carries type, default, range / options, application time, `group`, and `description`. Numbers must be
finite and within range, color channels 0..1. Values are rejected, never silently clamped. Groups consisting of
`advanced = true` fields start collapsed; search / Modified filter matches expand them. `applyAction` can name the
consuming action only for NextAction fields; it must be a registered action. Inspector shows a shared application time
once per group, differing times in each field's `*` marker / tooltip. This presentation metadata runs no setters or
actions; a value change is separate from Pending awaiting worker response. On Choice parameters, `choiceButtons = true`
shows options in Inspector as buttons highlighting the selected value instead of a dropdown, arranged in one or two
columns according to panel width; the default is `false`. A positive `choiceButtonColumns` caps the column count; as
space narrows, fewer columns keep buttons readable. The default `0` chooses one or two columns.

`SceneAction` carries identity, label, description, and optional `primary` (default `false`). At most one action may
be marked primary; its list position does not matter. Inspector shows it full-width in a fixed footer, with other
actions in an adaptive two-column group in scrollable content. A primary action is optional; in old catalogs without the
field, all actions are secondary. This metadata changes neither action callbacks nor worker behavior.

| Application time | Worker | Content |
| --- | --- | --- |
| `Immediate` | Calls the setter, accepts on success | Update current appearance |
| `NextAction` | Calls the setter, accepts on success | Store for the next trigger |
| `OnReset` | Validates and stores; does not call the setter | Use in the new factory |

Setters / actions report success with empty text, rejection with an explanation; a rejected setter does not change
content. The editor stores only accepted values for reopening; incompatible / removed fields in a new catalog are
dropped and explained. `CaptureParameters() const` reports authored controls changed by actions or product input;
runtime validates the entire report, and unknown / invalid fields change no values. There is no preset writing to disk,
reflection, or automatic source writing.

## Inspection

`EditorScene::Inspect()` returns read-only `SceneInspection`: focus rectangle, labels / values, table tabs, hidden /
disabled control and action IDs, `showImage`. It does not affect the recipe; changes are sent separately at most 10 Hz.
General UI displays it without knowing project IDs. `SceneReadout::kind` selects left-aligned wrapped text for default
`Text`, right-aligned technical font for `Number`, and a short badge for `Status`. Status text also wraps if it does not
fit. Units are part of the formatted value.

## Worker runtime

`RunEditorWorker(argc, argv, RegisterProjectScenes, settings)` takes an optional Game factory; the default is
`WorkerGame`. `WorkerSession` separates IPC, instance lifecycle, and frame transfer from Game; the project can set up
the same session in its own real Game (consumer's custom Game adapter). `WorkerSessionActions` wires quit / reset /
activate and application-command callbacks. Game hooks: `FrameSeconds` (host simulation time), `DrawSeconds`
(presentation clock), `SetScene(scene, true)` (restarts the same scene's Enter flow). The host setting `fixedWindow`
prevents Options from changing OS window / capture size.

## Project recipe and build

Project CMake generates a configuration-specific JSON recipe; select another with `EngineEditor.exe --project <path>`:

```json
{
  "version": 1,
  "name": "My scenes",
  "buildDirectory": "C:/MyProject/build",
  "worker": {"executable": "C:/MyProject/build/Debug/MyEditorWorker.exe", "arguments": [],
             "directory": "C:/MyProject/build/Debug", "environment": {}},
  "build": {"executable": "C:/Tools/CMake/bin/cmake.exe",
            "arguments": ["--build", "C:/MyProject/build", "--config", "Debug", "--target", "MyEditorWorker"],
            "directory": "C:/MyProject/build", "environment": {}}
}
```

Relative paths are relative to the recipe file; shell command text is not executed. `worker.executable` is build output;
the editor starts workers from snapshot copies of it. Build & Refresh: build only the recipe target while the running
scene stays open → snapshot on success → start a new worker beside the old one with the same state → replace when
ready ([Architecture](Architecture.md#worker-flow)). Build errors leave the scene unchanged. A Windows Job Object owns
only the process tree started by the editor; workers not exiting within 5 s are terminated. A mutex protects the same
build directory across editor sessions.

The general editor can build without Game:

```powershell
cmake -S Engine -B build/engine-editor -G "Ninja Multi-Config" -DENGINE_BUILD_RUNTIME=ON -DENGINE_BUILD_EDITOR=ON
cmake --build build/engine-editor --config Debug --target EngineEditor
```

`ENGINE_BUILD_EDITOR` and `ENGINE_BUILD_EDITOR_TESTS` default to OFF. Raylib 5.5, rlImGui
118221c8a532501a3c2c236b62950877cbefc2b4, and Dear ImGui v1.92.7-docking are pinned.

## IPC and image transfer

- Control: Windows named pipe (byte mode, `PIPE_NOWAIT`), 4-byte little-endian length + UTF-8 JSON. Messages up to 1
  MiB; send queue up to 256 messages / 8 MiB; 64 KiB read / write budget per poll. Poll only on the owning thread.
  Process stdout / stderr goes to Output; Full keeps the last 1000 lines, Last Build Try keeps the latest attempt's
  10000-line / 8 MiB buffer and first error. Full session / attempt logs are under `editor-logs/` in the build
  directory. On process exit, remaining output is fully drained regardless of the 64 KiB poll limit; buffer limits do
  not truncate output written to files.
- Envelope: version, session ID, content generation, request ID, type, data. Reset creates a new generation; worker
  startup creates a new session. Startup / first-frame timeout is 30 s.
- Image: two mutex-protected shared slots, up to 1920×1080 RGBA8. The canvas is read from the GPU, flipped, copied; the
  editor uploads it to its own texture. Busy buffers are not waited on; old frames may be skipped. The project chooses
  canvas size (dimensions supplied in GameSettings). Target: worker 30 FPS, editor 60 FPS; View menu metrics show
  readback / publish / transfer times (not full input→screen latency).
- When the protocol or `InputKey` layout changes, rebuild editor and worker together; version / key-count mismatches are
  reported as errors.

## Headless editor tests

```powershell
cmake -S Engine -B build/editor-tests -G "Ninja Multi-Config" -DENGINE_BUILD_RUNTIME=OFF -DENGINE_BUILD_EDITOR_TESTS=ON
cmake --build build/editor-tests --config Debug --target EngineEditorTests
ctest --test-dir build/editor-tests -C Debug --output-on-failure
```

Covers viewport / DPI mapping, input ownership, message fragmentation, version / session separation, parameter
validation / restoration, registration without resource setup, forbidding builds before worker exit, and shared buffers.
