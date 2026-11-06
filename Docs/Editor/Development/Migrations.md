# Cumulative migration notes

## One scene type (D53)

The editor's catalog opens the game's own `Scene`; the separate editor scene type is gone. Write each screen once:
derive it from `Scene`, take `const SceneServices&` plus its own values in the constructor, and call
`ShutdownNoThrow()` in its destructor. The game builds it from `Game::Services()`, the catalog's factory from the
services it receives.

| Old | New | How to migrate |
| --- | --- | --- |
| `EditorScene` (`Editor/Api/EditorScene.h`) | `Scene` (`Scene/Scene.h`) | Derive from `Scene`; add `~X() override { ShutdownNoThrow(); }`. A game scene and its editor copy become one class. |
| `HandleInput` with `input.cancelled` | `InputCancelled()` | Move the cancelled branch into an `InputCancelled` override; `HandleInput` no longer sees cancelled input. |
| `SceneDefinition::create` returning `std::unique_ptr<EditorScene>` | `std::unique_ptr<Scene>` | Return the game's scene class. |
| `Editor/Api/SceneParameter.h`, `SceneInspection.h`, `SceneServices.h` | `Scene/SceneParameter.h`, `Scene/SceneInspection.h`, `Scene/SceneServices.h` | Change the include. |
| `Editor/Api/SimulationHost.h`, `HostedSimulation.h` | `Simulation/SimulationHost.h`, `Simulation/HostedSimulation.h` | Change the include; a game scene keeps the same `HostedSimulation<T>` member (null host in the game). |
| A game scene taking `TimeScale&`, `SoundPlayer&` or `Game&` | `const SceneServices&` (`time`, `sounds`, `audio`, `effects`, `font`) | Pass `Services()` from the game's constructor. |
| `SceneServices::applicationCommand`, `SceneServices::profiler`, `WorkerSessionActions::application` | Removed | Nothing read them. |
| `WorkerSession(game, settings, registry, channel, session, font, audio, sounds, actions)` | `WorkerSession(game, settings, registry, channel, session, actions)` | Services come from `game.Services()`. |
| `WorkerSession::Simulation()` | `WorkerSession::Services()` | Build a custom adapter's scenes from it. |
| `RegisterEditorScene`, `AddRegisteredEditorScenes` | `RegisterCatalogScene`, `AddRegisteredCatalogScenes` | Rename the call. |
| `WorkerScene::CaptureParameters()` | `WorkerScene::MergeCapture()` | Worker code only. |

## 0.3.0 — editor scenes (tools contract 2)

An editor scene is a catalog entry displayed in the viewport. It is distinct from the engine's `Scene`.
These historical names are recorded here for source and automation migrations. Runtime behavior is unchanged.
Rebuild editor and worker together.

| Old name | New name |
| --- | --- |
| `EditorPreview`, `EditorPreview.h/.cpp` | `EditorScene`, `EditorScene.h/.cpp` |
| `PreviewDefinition`, `PreviewRegistry`, `PreviewServices` | `SceneDefinition`, `SceneRegistry`, `SceneServices` |
| `PreviewParameter(s)`, `PreviewParameterKind`, `PreviewValue` | `SceneParameter(s)`, `SceneParameterKind`, `SceneValue` |
| `PreviewApplyMode`, `PreviewColor`, `PreviewAction` | `SceneApplyMode`, `SceneColor`, `SceneAction` |
| `PreviewInspection`, `PreviewReadout(Kind)`, `PreviewContentBounds` | `SceneInspection`, `SceneReadout(Kind)`, `SceneContentBounds` |
| `PreviewTable`, `PreviewBenchmark(Variant)` | `SceneTable`, `SceneBenchmark(Variant)` |
| `ValidatePreviewValue`, `ValidatePreviewParameter` | `ValidateSceneValue`, `ValidateSceneParameter` |
| `PreviewDefaults`, `MergePreviewCapture`, `RestorePreviewParameters` | `SceneDefaults`, `MergeSceneCapture`, `RestoreSceneParameters` |
| `Encode/DecodePreviewParameters/Value/Catalog/Inspection` | `Encode/DecodeSceneParameters/Value/Catalog/Inspection` |
| `Editor/Preview/` | `Editor/Worker/` |
| `PreviewHost`, `RunPreviewHost` | `WorkerHost`, `RunEditorWorker` |
| `PreviewGame`, `PreviewScene`, `PreviewSession(Actions)`, `PreviewProfiler` | `WorkerGame`, `WorkerScene`, `WorkerSession(Actions)`, `WorkerProfiler` |
| `PreviewApplicationFactory`, `EnginePreviewRuntime` | `WorkerApplicationFactory`, `EngineWorkerRuntime` |
| `PreviewWorker(Mode)`, `PreviewBuild*` | `WorkerProcess(Mode)`, `WorkerBuild*` |
| `PreviewProtocol*`, `PreviewMessage*`, `PreviewMax*`, `PreviewMinPlaybackSpeed` | `WorkerProtocol*`, `WorkerMessage*`, `WorkerMax*`, `WorkerMinPlaybackSpeed` |
| `PreviewOpenState`, `PreviewTuningState/Status/StateNames` | `WorkerOpenState`, `WorkerTuningState/Status/StateNames` |
| `Encode/DecodePreviewMessage/Input/Open/Tuning` | `Encode/DecodeWorkerMessage/Input/Open/Tuning` |
| `PreviewTimestamp/Utf8/Wide`, `ThrowPreviewWindowsError`, `NewPreviewSession` | `WorkerTimestamp/Utf8/Wide`, `ThrowWorkerWindowsError`, `NewWorkerSession` |
| `EditorPreviewImage`, `PreviewDisplay`, `PreviewPixels` | `ViewportImage`, `ViewportDisplay`, `ViewportPixels` |
| `PreviewViewport`, `FitPreviewViewport`, `MapPreviewPoint` | `SceneViewport`, `FitSceneViewport`, `MapViewportPoint` |
| `PreviewInputRouter`, `AccumulatePreviewInput`, `ConsumePreviewInput` | `ViewportInputRouter`, `AccumulateViewportInput`, `ConsumeViewportInput` |
| `cancelPreviewInput`, `FrameRegion::PreviewBridge` | `cancelViewportInput`, `FrameRegion::WorkerBridge` |
| `FailPreview`, `TakeOpenedPreview`, `_openedPreview` | `FailScene`, `TakeOpenedScene`, `_openedScene` |
| `CharacterPreview`, `RecipeCharacterPreview`, `CharacterPreview.md` | `CharacterScene`, `RecipeCharacterScene`, `CharacterScene.md` |
| `CreatingPreviews.md` | `CreatingScenes.md` |
| Default project worker target `<Project>Preview` | `<Project>EditorWorker` |
| `ctl` JSON `preview` / `previews` | `scene` / `scenes` |
| Editor control protocol 2 | 3 |

Files follow their renamed classes. Sample content classes use `FirstScene`, `MyScene` and
`RecipeCharacterScene`; worker application IDs and executable examples use Worker names.
Catalog IDs, favorites, recent history and saved settings keys are stable. The current settings store has no key
containing the old term, so no legacy-key fallback is needed. Unrelated progress-bar slices, audio review artifacts,
ImGui identifiers and prior history keep their existing terminology.

### Viewport image decision

`EditorPreviewImage` carries the uploaded viewport frame and its transport metrics, rather than a content object's
owned image. It therefore becomes `ViewportImage`.
