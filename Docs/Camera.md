# Game camera

A camera that follows the player, frames several players at once, stays inside the level and can split the screen. It is
made after Godot's `Camera2D` and Cinemachine's follow camera, so its words will be familiar: dead zone, look-ahead,
smoothing, limits, target group. Design record: `Docs/Design/README.md` → D35 (engine repository). Worked example:
[AgentDocs/Recipes/GameCamera.md](../AgentDocs/Recipes/GameCamera.md).

## The parts

| Header | Use |
| --- | --- |
| `Camera/Camera2D.h` | One camera: follow, zoom, limits, the view to draw through, canvas ↔ world |
| `Camera/Cameras.h` | A scene's cameras: `Main()`, split screen, which camera is under the mouse |
| `Camera/CameraSettings.h` | `CameraFollow`, `CameraZoom`, limits, pixel snapping |
| `Camera/CameraTarget.h` | A point to keep in view, with a weight and a radius |
| `Camera/CameraPose.h` | Where a camera is: centre, zoom, rotation; an animation pose |
| `Camera/CameraView.h` | What one camera shows this frame; `paint.Camera(view)` draws through it |
| `Camera/Debug/CameraGuides.h` | `DrawCameraGuides`: the dead zone, limits, targets and aim on screen |
| `Animation/SmoothDamp.h` | The critically damped follow the camera uses, for anything else that should glide |

`Core/Camera` computes and draws nothing (only `Debug/` draws), and builds with precise floats, so a simulation may keep a
camera. `python Engine check` holds it to that.

## Following

```cpp
Cameras _cameras{canvas, {.follow = {.deadZone = {0.2f, 0.3f}, .smoothTime = 0.15f, .lookAhead = {0.4f, 0.0f}},
                          .limits = levelBounds}};

// Update, after the simulation advanced:
_cameras.Main().Follow(_simulation.Player().At(_runner.Alpha())); // Where the player is drawn this frame
_cameras.Update(seconds);

// Draw:
auto world = paint.Camera(_cameras.Main().View());
DrawWorld(paint);
```

Each `Update` runs the same rules in order:

| Rule | Setting | What it does |
| --- | --- | --- |
| Targets | `Follow(point)` or `Follow({targets})` | The weighted middle of the targets; weight 0 leaves one out |
| Offset | `follow.offset` | World units added: look above a character's feet |
| Look-ahead | `follow.lookAhead`, `lookAheadLimit`, `lookAheadSmoothTime` | Seconds of the targets' motion to look ahead, per axis, eased in and out, at most a fraction of the view |
| Dead zone | `follow.deadZone` | A box around the middle, as a fraction of the view, that the aim moves in without moving the camera |
| Fitting | `zoom.fit` | Moves the camera so every target's circle stays in view |
| Limits | `limits` | The view stays inside; centred on an axis where the level is smaller than the view |
| Smoothing | `follow.smoothTime` | Glides to where it is heading without overshooting (`SmoothDamp`); 0 follows at once |

Targets are copied values, not pointers: call `Follow` with the current positions before each `Update`. Use the positions
the view draws (`StepSmoothed::At(alpha)`), so the camera moves as smoothly as the player is drawn. `Update` takes the
scene's game seconds, so a hitstop holds the camera too.

`Snap()` arrives at once and forgets motion: call it after the first `Follow` of a level, after a teleport, after a cut.
`StopFollowing()` leaves the camera where it is; `SetPose(pose)` places it by hand, so a `SecondsTween<CameraPose>` or a
clip of `CameraPose` makes a cutscene move. Limits still apply.

## Several players on one screen

```cpp
.zoom = {.fit = true, .min = 0.5f, .max = 1.0f, .padding = 120.0f}
_cameras.Main().Follow({{.position = one, .radius = 24.0f}, {.position = two, .radius = 24.0f}});
```

With `fit`, the camera zooms so that every target's circle has `padding` canvas units to spare, between `min` (the
farthest out) and `max`. Weights pull the middle towards one player, but fitting still keeps the others in view. When the
players are too far apart even at `min`, the camera centres between them and the rest leave the frame: the game decides
whether that may happen. To forbid it, the rules clamp the players; a camera in the rules ([below](#a-camera-in-the-rules))
gives them the exact visible area, `VisibleArea()`.

## Split screen and the main camera

`Cameras` is the place to ask "which camera?", like Unity's `Camera.main`. `Main()` is the one a single-screen game uses;
`Split(count, layout, gap)` gives 1 to 4 cameras a part of the canvas each (`SideBySide`, `Stacked`, `Grid`). Cameras that
exist keep their place and targets; new ones start as copies of the main camera, so a split opens without a jump.

```cpp
_cameras.Split(2, CameraSplit::SideBySide, 6.0f);
_cameras[0].Follow(one);
_cameras[1].Follow(two);
_cameras.Update(seconds);
for (const Camera2D& camera : _cameras) {
    auto world = paint.Camera(camera.View()); // Clipped to this camera's viewport
    DrawWorld(paint);
}
// The mouse in the world, through whichever view it is over:
if (const Camera2D* under = _cameras.At(mouse)) aim = under->ToWorld(mouse);
```

The world is drawn once per camera. `SetCanvas` follows a window that changed size. Screen overlays (`FxStage::DrawOverlay`,
the HUD) are drawn once, after the loop, in canvas coordinates.

## Effects over a game camera

`FxStage::WorldCamera(paint, view, limits)` puts a cinematic shot, shake, punch, kick and motion on top of a camera's view.
Effect positions are then world positions. A shot blends from the view by its weight and keeps the view's place on the
canvas, so each part of a split screen keeps its own; with `keepInArena` it stays inside `limits`:

```cpp
auto world = paint.Camera(_fx.WorldCamera(paint, camera.View(), camera.Settings().limits));
```

The stage has one shake, so every view of a split screen shakes together.

## A camera in the rules

A camera usually decides nothing and lives in the view, on seconds. Some games make it part of the rules: enemies wake
when the view first shows them, the screen never scrolls back, the player may not leave the view. Then the camera belongs
in the simulation, and replays must reproduce it:

- Keep a `Camera2D` member; its viewport is the game's canvas size, a constant of the game.
- `Follow` and `Update(step.seconds)` in `Step`. The math is the same as in a view, deterministic on fixed steps.
- Name it in `State`: `state.Field("camera", _camera);`, or keep it in the simulation's [state struct](Simulation.md#state). Settings are data; if the rules change
  them (moving limits), derive them from state the simulation already names.
- Read `VisibleArea()` or `Shows(point, radius)` in the rules.
- Draw with `camera.View(runner.Alpha())`, between the last two steps, as `StepSmoothed` does.

## Seeing it

`DrawCameraGuides(paint, camera)` draws, in canvas coordinates after the world: the viewport, the dead zone around where the
camera is heading, the limits, each target's circle, the aim with look-ahead (a cross) and the point it heads for (a dot).
It is view code; a simulation cannot include it. An editor scene usually shows it behind a Boolean parameter.

## Details

- **Pixel snapping:** `pixelSnap` moves the world by whole canvas units only, so code-drawn lines do not shimmer on a
  slowly moving camera.
- **Rotation:** `CameraPose::rotation` turns the view. Limits, `VisibleArea` and fitting ignore it.
- **Coordinates:** a camera starts centred on its viewport at zoom 1, where world and canvas coordinates are the same.
  `ToWorld` and `ToCanvas` convert; `Shows` asks whether a point or circle is in view.
- **Settings:** `IsValid(settings)`; invalid ones throw where they are given.

| Godot `Camera2D` | Unity / Cinemachine | Here |
| --- | --- | --- |
| `drag_*_margin` | Dead zone | `follow.deadZone` |
| `position_smoothing_speed` | Damping | `follow.smoothTime` (`SmoothDamp`) |
| — | Lookahead time / smoothing | `follow.lookAhead`, `lookAheadSmoothTime` |
| `offset` | Tracked object offset | `follow.offset` |
| `limit_*` | Confiner 2D | `limits` |
| `zoom` | Orthographic size | `zoom.zoom` |
| — | Target group + group framing | `Follow({targets})`, `zoom.fit` |
| `reset_smoothing()` | `OnTargetObjectWarped` | `Snap()` |
| `get_screen_center_position()` | — | `Pose().center` |
| `process_callback = Physics` | Update method: fixed | `Update(step.seconds)` in a simulation |
| Current camera | `Camera.main` | `Cameras::Main()` |
| `Viewport` + `SubViewport` | Camera rect | `Cameras::Split`, `Camera2D::Viewport()` |
