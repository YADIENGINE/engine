# Developing editor scenes

## Choosing a content type

For simple drawing, `EditorScene::Draw(Painter&) const` is enough. Add `Update(dt)` for animation/state, `HandleInput`
for user interaction, and `ExecuteAction` for triggers. Each catalog entry is explicitly registered with a
`SceneDefinition` and factory. The general engine does not register scenes by scanning files automatically. Projects
can write their own short registration/gallery helpers (such as quick drawing or gallery scenes); these are not required
to use the EngineEditor API.

The scene calls the production model/View. Do not write a second editor version of the same appearance. List/gallery
layout and project-local short registration helpers belong in the consumer's development layer.

## Resources and lifecycle

1. The registry contains only metadata and create callbacks; global registration does not create GL/fonts/audio.
2. The factory constructs the instance on the engine thread with a complete parameter map when services are ready.
3. ObjectList manages Start/Update/Draw/Stop; content does not dispatch these hooks manually.
4. Reset creates a new instance with the accepted recipe; old resources are cleaned up while the context is alive.

`SceneServices` font/audio references outlive the instance. Views explicitly own textures; dependent effects are
cleaned up before the texture. Stop/destructors do not throw. Factory/hook exceptions may close the worker.

## Control design

Author parameter type, default, bounds, group, and description in one definition. Identity is stable; favorites/history
and accepted overrides depend on it. Float values are `double`, integers `int64_t`, Choice values are option strings.
`Immediate` changes appearance; `NextAction` prepares the next trigger; `OnReset` is used in the new instance. The
constructor reads initial values itself; do not assume setters are called during initial setup.

Provide an explicit seed parameter for random behavior. If Randomize/product input changes an authored control, report
it through CaptureParameters. Live counters, transient animation, and error descriptions belong in Inspect, not in the
recipe. A setter returning a rejection explanation must not change current content.

## Clock, input, and real game flow

Decorative time is `paint.Time()`; state advances with Update dt. Do not wire scene time into Views manually; the host
applies Pause, Step, and speed. Arbitrary-time scrubbing is unsupported; use Reset + Step for stateful scenes. Product
input arrives when the View area is focused; do not reread Inspector focus or OS input.

RunEditorWorker can take a custom Game factory for a scene running the real game. `WorkerSession` carries control,
lifecycle, and frame transfer; game scenes stay in the same Game::Run loop. Transient profiles, save isolation, and test
flow are the consumer's responsibility. `persistentOptions=false` alone does not prevent game progress/telemetry writes.
Instead of adding Editor includes or editor-specific conditional code to production Game, design general, shareable
scene/View entry points.

Full value and IPC contract: [API](Api.md). Validation: [Testing](Testing.md).
