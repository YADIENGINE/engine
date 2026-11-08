# Runtime and lifecycle

## Lifecycle

The headless lifecycle type is `Scene/Scene.h`; the engine-private `Scene/Internals/SceneManager.h` selects among
scenes. `Runtime/Game.h` connects them to application services; games select scenes through `Game::RegisterScene` /
`SetScene`.

A `Scene` keeps its simulation and views as value members and calls them from its hooks, in the order it wants; there
is no object list ([recipe](../AgentDocs/Recipes/SceneMembers.md)). Things that spawn and die live in a `Pool<T>`
([Simulation](Simulation.md#things-that-come-and-go)). The hooks:

- `Start` once, before the first `Enter`; `Stop` once, at shutdown, when `Start` ran.
- `Enter` at the start of every visit (each activation, a restart included); `Exit` at its end, after `InputCancelled`.
- `UpdateReal(realSeconds)` every frame, in a hitstop and a pause too; `Update(seconds)` on the scene's clock
  (`TimeDomain::Game` by default, so hitstop and pause reach it; `TimeDomain::Real` for a screen that must keep
  moving through them), skipped when no time passed; `Draw(Painter&) const` after `Enter`.
- `HandleInput` and `InputCancelled` ([below](#input-cancellation)).

`SetScene` queues a change; activation happens between callbacks, before the next scene update. A hook that throws is
not retried: the flag that pairs it is set first, so `Exit` and `Stop` still run for an `Enter` or `Start` that failed.
Lifecycle, input, update and draw dispatch cannot be entered recursively; use `SetScene` from callbacks instead of
calling Activate/Deactivate/Shutdown. Cancellation is the exception: an input callback may cancel.

The host shuts a scene down while it is alive, so a scene needs no destructor. `Game::Run` shuts down all registered
scenes before returning or rethrowing, preserving the original runtime exception if cleanup also fails. A scene is only
started by `Game::Run` (or the editor's worker), so one destroyed before that, for example when a derived game's
constructor throws, has nothing to undo. `Shutdown()` is idempotent and completes cancellation, `Exit` and `Stop`
before rethrowing the first failure. Shutdown scenes cannot be selected or activated again. Code that drives a scene
itself, such as a test, calls `Shutdown()` before the scene dies; debug builds assert when a started scene is destroyed
without it. Scenes are value members of the derived game; base engine services outlive those members.

`SceneManager` borrows registered scenes and owns the current/pending selection. `Request` only queues a target; the
last request before a commit wins. `Commit` consumes that request and returns whether a change occurred. Its
`beforeChange` callback runs before departure, only for an actual change; Game uses it to cancel physical input and
saves options after successful activation. A request made during Exit/Enter waits for a later commit. A same-scene
request is a no-op unless restart is true. Shutdown visits scenes in reverse registration order, completes cleanup
before rethrowing the first failure, and rejects further registrations/selections. The manager's destructor does not
dereference borrowed scenes; the application must call Shutdown while they are alive, as Game::Run does.

The frame order is device sampling → game input → audio/game real-time update → scene change → `Scene::UpdateReal` →
normal real-domain scene tick → pause/scaled-time calculation → normal game-domain scene tick → scene draw → game
overlay draw → completed canvas → window presentation. `ReadInput()` defaults to normal device/canvas sampling and can
supply external `DeviceInput` snapshots. Device polling and window-coordinate samples are private to Runtime; public
input uses logical canvas coordinates. The editor applies the same normalization with its own viewport mapping. See
[Input](Input.md). `CanvasReady(const Texture2D&)` receives a borrowed completed canvas before presentation; retain no
texture ownership. `DrawWindow(Painter&)` draws after canvas presentation, inside the window drawing pass; tools render native
ImGui in `DrawToolOverlay()`, which runs after that drawing executed.
`Renderer::Frame::Complete` invokes these callbacks explicitly; scoped-frame cleanup never invokes user callbacks.
Neither hook changes scene ownership. `SetPaused` or the active scene's `GameTimePaused()` freezes game time;
`Time()` handles hitstop and slow motion. Real-time screens and game overlays keep advancing.

## Input cancellation

`Scene::InputCancelled()` clears scene-local gestures and pending presses. Engine calls it on scene departure, local
window focus loss and incoming `DeviceInput::cancelled` snapshots. Such snapshots are cancellation barriers, not normal
release events, and never reach action handlers. Cancellation may repeat across frames; hooks must be idempotent and
clear held actions as well as gestures. The protected `Game::CancelInput()` also cancels local device input; games may
call it when a visual transition starts. Cancellation is allowed inside an input callback; other lifecycle dispatch
remains guarded.

Runtime keeps sampling touch IDs while suppressing actions until every held key/contact has been released and the
neutral frame has been consumed. External `ReadInput()` overrides own their focus/cancellation policy, so hidden worker
windows remain independent of OS focus. Pointer helpers accept canonical empty cancelled snapshots; forward one from
InputCancelled to cancel and drain the old gesture without losing the next new press.


## Startup, loading, and error boundary

The `Game` constructor sets up renderer/asset/font services, activates the theme, and starts loading the audio catalog
in the background. If persistent options are enabled, they are loaded and applied before `Start()`. `Start()` selects
the first scene; do not call playback here, audio is not ready yet. The default entry point reports startup exceptions;
content hooks define their own error policy.

Between `Start()` and the first scene the loading screen runs. A game gets the engine's "Made with" screen: about two
seconds, longer if the sounds take longer, and any key, mouse button or touch skips it once they are ready. Its look is
the `loadingScreen` [project setting](ProjectSettings.md#settings): Default, Horror, Rpg, SciFi, Arcade or Cozy. To show
a screen of your own, derive from `LoadingScene`, register it, and call `SetLoadingScene` from `Start()`; it replaces
the setting, and `SetScene` is called as usual and waits. A game that wants no screen gives an empty `LoadingScene`,
which closes when the sounds are ready.

A `LoadingScene` runs on real time and is an ordinary scene otherwise. `Loaded()` turns true when the sounds are ready;
playing a sound before that throws, so the waiting part is silent, and a closing animation may play sounds or start
music that carries into the first scene. Override `Finished()` for a closing animation; the engine asks it only after
`Loaded()`, so a screen cannot close before the sounds are ready. While it runs, the game's own `Update`, `HandleInput`
and `Draw` do not. The first scene's `Start` and `Enter` always find the sounds ready. If the player closes the window
during loading, `Game::Stop()` still runs, but the first scene never started. Tools and editor hosts show no screen and
only wait for the sounds.

## Input and time selection

`Game::HandleInput` forwards to the active scene by default. If overriding it, call `Game::HandleInput(input)` at the
appropriate point to preserve scene input. `Game::Update` receives real time; the scene chooses `TimeDomain::Real` or
`Game` for its normal `Update(float)`. The scene's `UpdateReal(float)` hook runs first regardless of the chosen
domain. It is empty by default. Like other scene dispatches, `TickReal` is guarded against reentry and runs only on
the active scene. Zero time may complete startup/entry
work but does not advance the UpdateReal callback.

`GameTimePaused()` defaults to false; the scene owning pause state overrides it. The decision is read after UpdateReal:
if a closing animation finishes that frame, the game tick can resume that frame. Game's `SetPaused` decision is ORed
with it; for example, while a paused game is in the background on another screen, the application can freeze the shared
game clock. TimeScale.Advance is not called while paused; hitstop/slow motion durations are preserved too. Pausing
real-time menus requires a separate policy. `Time()` applies hitstop/slow motion. A scene reaches what its feedback needs
through the `SceneServices` its constructor takes (`Game::Services()`): `sounds`, `effects` (the player's effect choice),
`time`, `audio`, `font` and `canvas`; `UpdateReal`'s argument is this frame's real duration (`Game::RealSeconds()`). A
[Stage](Graphics/Stage.md) or an `FxStage` takes it there (`UpdateReal`) and game time in `Update`, since `Update` does not
run while game time stands still. `FrameSeconds` customizes the host frame duration; `DrawSeconds` customizes presentation time. Decorative View drawing uses
[Painter::Time](Graphics/Painter.md). UpdateReal also receives time after the FrameSeconds transformation; the editor's
pause/speed/single-step apply to this hook.

Gameplay that must replay runs as a [simulation](Simulation.md#hosting-it-in-a-scene): a scene in `TimeDomain::Game` hands
the scaled seconds to a `SimulationRunner`, which runs fixed steps. This frame order does not change, and hitstop and slow
motion only change how many steps run per real second.

`SetScene` does not destroy a scene in the middle of a callback; it queues the transition. `SetScene(scene, true)`
allows reentry to the same scene. Model/View state requiring a reset must be cleared explicitly in `Enter`; `Start` is
not called again. The transition commit follows Game.Update; a change requested in a scene callback may wait for the
next frame's commit. A scene's members survive its departure; they live as long as the scene.
