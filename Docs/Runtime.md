# Runtime and lifecycle

## Lifecycle

The headless lifecycle types live in `GameObjects/GameObject.h`, `GameObjects/ObjectList.h`, `Scene/Scene.h` and the
engine-private `Scene/Internals/SceneManager.h`. `Runtime/Game.h` connects them to application services; games select
scenes through `Game::RegisterScene` / `SetScene`.

`GameObject` supplies overridable `Start`, `Update(float)`, `Draw(Painter&) const`, and `Stop` hooks. `ObjectList` calls
`Start` once before an enabled object's first update/draw, skips disabled objects, and calls `Stop` once on removal or
shutdown. A stopped object is finished; create a new object to replace it. Zero time starts new objects but skips
updates, while drawing remains available.

An `ObjectList` can register existing value members by reference or own dynamic objects passed as
`std::unique_ptr<GameObject>`. Each object belongs to one list. Additions take effect on the next start/update dispatch.
Removal skips further callbacks immediately; stopping and destruction happen at the next dispatch or shutdown. Callbacks
may add/remove objects, but must not recursively dispatch or shut down their own list. The same guard covers drawing,
removal callbacks and shutdown. `Stop()` detaches and destroys all entries even if a callback throws, then rethrows the
first failure. Destructors report cleanup failures to stderr without propagating them. Debug builds assert if an object
dies while still registered.

`Scene` is a `GameObject` with its own object list, input handler, and `Enter`/`Exit` hooks. Register value members with
`AddObject(GameObject&)`, or transfer ownership with `AddObject(std::unique_ptr<GameObject>)`. `RemoveObject` and
`ClearObjects` immediately suppress further callbacks, then stop/destroy removed children at the next list dispatch or
shutdown. Clearing children does not shut down the scene itself. A reference remains the caller's responsibility until
removal is committed; a reference returned for an owned object is invalid after its destruction. An inactive/disabled
scene defers removal until its next eligible dispatch or shutdown. The scene's `Start` runs once; `Enter`/`Exit` run on
every visit. Children update before the scene's update and draw before its draw, allowing presentation to advance before
the scene emits new simulation events. Scenes choose `TimeDomain::Real` or `TimeDomain::Game`. Disabled scenes skip
input/update/draw for the entire scene. `SetScene` queues a change; activation happens between callbacks, before the
next scene update. Activating a disabled scene defers its Start/child Start/Enter sequence until it is enabled and next
receives input or a tick; it does not draw before Enter. Scene lifecycle/input/update/draw dispatch cannot be entered
recursively. Use `SetScene` from callbacks instead of calling Activate/Deactivate/Shutdown.

Registered references must outlive their registration. A concrete scene's destructor must call `ShutdownNoThrow()` while
its value members and virtual hooks are still alive (see the scene in [GettingStarted](GettingStarted.md)). Shutdown is
idempotent. Explicit `Shutdown()` completes cancellation, Exit and all Stop callbacks before rethrowing the first
failure; `ShutdownNoThrow()` reports that failure without throwing from a destructor. `Game::Run` shuts down all
registered scenes before returning or rethrowing, preserving the original runtime exception if cleanup also fails.
Shutdown scenes cannot be selected or activated again. Scenes are value members of the derived game; base engine
services outlive those members.

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
Neither hook changes scene ownership. `SetPaused` or the active enabled scene's `GameTimePaused()` freezes game time;
`Time()` handles hitstop and slow motion. Real-time screens and game overlays keep advancing.

## Input cancellation

`Scene::InputCancelled()` clears scene-local gestures and pending presses. Engine calls it on scene departure, local
window focus loss and incoming `DeviceInput::cancelled` snapshots. Such snapshots are cancellation barriers, not normal
release events, and never reach action handlers. Cancellation may repeat across frames; hooks must be idempotent and
clear held actions as well as gestures. The protected `Game::CancelInput()` also cancels local device input; games may
call it when a visual transition starts. Cancellation is allowed inside an input callback; other lifecycle dispatch
remains guarded.

Runtime keeps sampling touch IDs while suppressing actions until every held key/contact has been released and the
neutral frame has been consumed. External `ReadInput()` overrides own their focus/cancellation policy, so hidden preview
windows remain independent of OS focus. Pointer helpers accept canonical empty cancelled snapshots; forward one from
InputCancelled to cancel and drain the old gesture without losing the next new press.


## Startup, loading, and error boundary

The `Game` constructor sets up renderer/asset/font services, activates the theme, and starts loading the audio catalog
in the background. If persistent options are enabled, they are loaded and applied before `Start()`. `Start()` selects
the first scene; do not call playback here assuming audio is ready. The loading screen draws the first scene without
advancing simulation/input; customize presentation through `DrawLoading(paint, seconds, outro)` and
`LoadingOutroSeconds(loadSeconds)`. Normal scene flow starts after loading. The default entry point reports startup
exceptions; content hooks define their own error policy.

## Input and time selection

`Game::HandleInput` forwards to the active scene by default. If overriding it, call `Game::HandleInput(input)` at the
appropriate point to preserve scene input. `Game::Update` receives real time; the scene chooses `TimeDomain::Real` or
`Game` for its normal `Update(float)` and child updates. The scene's `UpdateReal(float)` hook runs first regardless of
the chosen domain; it does not call normal child updates. It is empty by default. Like other scene dispatches,
`TickReal` is guarded against reentry and runs only on the active/enabled scene. Zero time may complete startup/entry
work but does not advance the UpdateReal callback.

`GameTimePaused()` defaults to false; the scene owning pause state overrides it. The decision is read after UpdateReal:
if a closing animation finishes that frame, the game tick can resume that frame. Game's `SetPaused` decision is ORed
with it; for example, while a paused game is in the background on another screen, the application can freeze the shared
game clock. TimeScale.Advance is not called while paused; hitstop/slow motion durations are preserved too. Pausing
real-time menus requires a separate policy. `Time()` applies hitstop/slow motion. `FrameSeconds` customizes the host
frame duration; `DrawSeconds` customizes presentation time. Decorative View drawing uses
[Painter::Time](Graphics/Painter.md). UpdateReal also receives time after the FrameSeconds transformation; preview
pause/speed/single-step apply to this hook.

`SetScene` does not destroy a scene in the middle of a callback; it queues the transition. `SetScene(scene, true)`
allows reentry to the same scene. Model/View state requiring a reset must be cleared explicitly in `Enter`; `Start` is
not called again. The transition commit follows Game.Update; a change requested in a scene callback may wait for the
next frame's commit. Owned children also survive scene departure; they are destroyed on permanent removal or shutdown.