# Input and navigation

## Public API and ownership

Input is engine runtime infrastructure. Supported public headers:

| Header | Contract |
| --- | --- |
| `Input/Input.h` | `DeviceInput`, `InputKey`, `InputTouch`; raylib-free snapshot |
| `Input/InputNavigation.h` | `InputNavigation` reading a snapshot by reference |
| `Input/DragPointer.h` | Free dragging and tap on release |
| `Input/InputBindings.h` | Keys to per-player actions for a [simulation](Simulation.md#input-and-tapes) |
| `Input/CollectionPointer.h` | Collection tap, hover, and row scrolling |

Games and project editor code use only these headers. `Input/Internals/` is not supported API. Device reading, window
coordinates, normalization, and the shared contact tracker stay there. Gesture classes own their state and cannot be
copied. A snapshot is a plain value type. The headless implementation is in `EngineHeadless`; only raylib device reading
goes into the `Engine` target.

`Game::ReadInput()` reads devices once per host frame and produces a snapshot using the canvas transform supplied by
runtime. Sampling continues during transitions. The engine editor uses the same normalization with its viewport
transform and its own focus/ownership rules. External hosts can supply `DeviceInput` from a `ReadInput()` override. All
public positions use logical canvas coordinates; a touch outside the canvas retains its `id`, with an empty `position`.
Scenes do not reread devices or raylib input.

## Physical snapshot and semantic queries

`DeviceInput` carries key pressed/down/released states, mouse, wheel, and identified touch samples. `ShiftDown()` is
computed from keys. The mouse buttons are keys too: `InputKey::MouseLeft`, `MouseRight` and `MouseMiddle` answer
`Pressed`, `Down` and `Released` wherever the pointer is, so an input map binds them like any key. `mousePressed`,
`mouseDown` and `mouseReleased` stay as the left button's fields and agree with `MouseLeft`. `mousePosition` is the current position; `mousePress` is an eligible mouse press; `mouseHover` is populated
only on mouse movement, so a stationary mouse does not steal keyboard selection back. `pressPoints` carries newly
started contacts; `heldPoints` carries contacts still held. While touch is present and until subsequent synthetic mouse
events drain, mouse input is not added to these combined lists; `mousePress`/`mouseHover` are suppressed. A finger
entering the canvas after starting outside does not create a new press.

`InputNavigation(input)` is a semantic adapter that does not copy the snapshot. The snapshot must outlive the adapter;
do not retain the adapter between callbacks. Constructing it from a temporary snapshot is a compile error. `Device()`
gives the underlying snapshot by const reference; `PressPoints()`/`HeldPoints()` return borrowed spans.

| Query | Default |
| --- | --- |
| `Confirm()` | Enter / Space press |
| `Back()` | Escape |
| `Horizontal()`, `Vertical()` | WASD / arrow press, -1 / 0 / 1 |
| `TabStep()` | Q previous; E / Tab next |
| `Pressed(Direction)` | WASD / arrow press for that direction |
| `PressPoint()` | First eligible contact start; not a tap on release |
| `AnyPressed()`, `AnyHeld()` | Any key press / held key or contact |
| `HoverPoint()`, `MousePosition()`, `Wheel()` | Mouse movement / current position / wheel |

Use `Down(InputKey)` for continuous movement and `Pressed(InputKey)` for an action triggered once. Per-player bindings,
rebinding, confirmation sounds, and game actions are defined in the game. A Scene `HandleInput` override receives a
snapshot; scenes do not reread raylib.

`DeviceInput::cancelled` is not a normal release: pending contacts/actions are cancelled. Runtime forwards this snapshot
to the Scene `InputCancelled()` hook. In the hook, supply an empty `DeviceInput{.cancelled = true}` to the pointer
helper and clear local press/drag visuals. `Game::CancelInput()` notifies the scene and resynchronizes runtime's local
normalization. Touch IDs continue to be sampled; only cancelled snapshots are produced until all held inputs are
released and the neutral frame is consumed. An external host must provide the same focus/cancellation contract.

## Collections and drag-and-drop

`CollectionPointer` lives in EngineHeadless and contains no rendering or content policy.
Create a pointer helper with the grid bounds and row-drag threshold, then call `Update(DeviceInput)` once per input
frame. Its result contains a release `tap`, eligible `hover`, and signed `rows` (positive moves down the collection).
Touch movement emits rows while held. Crossing the movement threshold latches drag detection until release, including
when the contact returns to its origin. The first touch ID owns the gesture; later contacts cannot inherit it. Touch
also suppresses synthetic mouse events until all contacts and mouse buttons are quiet. Mouse taps activate on release;
wheel input applies only over the grid. Call `Cancel()` for a local gesture cancellation, then continue supplying input
frames on return so the cancelled contact can drain. Keyboard bindings, audio and confirmation remain in Game.

`Input/DragPointer.h` provides `DragPointer(threshold)` for free dragging. The `Update(input)` result contains
`pressed`, `position`, `origin`, `released`, `hover`, `dragging`, `tap`, and `cancelled`. Positions use canvas
coordinates. Returning to the origin after crossing the threshold does not turn the movement into a tap; the primary
touch ID is not transferred to other fingers. Synthetic mouse events from touch are suppressed. For local cancellation,
call `Cancel()` and continue supplying subsequent snapshots so the cancelled contact can drain. `released` alone does
not mean tap. A contact starting or moving outside the canvas is cancelled; no tap/drop is produced at the last inside
position. On a `cancelled` result, the consumer must clear its pending press and drag visuals.

Both gestures use the same private contact tracker in their own instances: primary identity is not transferred, leaving
the canvas cancels, and synthetic mouse input is suppressed. `CollectionPointerResult` also reports `cancelled`.
Threshold and scroll policy belong to the respective gesture. An empty cancelled snapshot in the `InputCancelled()` hook
clears the local contact and consumes the cancellation barrier in the same call; the next new press is not lost.

## Editor transport

The editor protocol carries snapshot keys (mouse buttons included), mouse fields, combined contact lists, and identified touches; an empty touch
position preserves identity outside the canvas. Duplicate touch IDs and capacity overflow are rejected. Worker
accumulation preserves press/release flags, accumulates `pressPoints`, and takes down/held/touch state from the latest
snapshot. Cancellation clears old actions and is delivered as a separate barrier before new actions. This transport
merges snapshots; it is not an ordered log of all physical events. A mouse button reaches the scene when pressed over
the viewport, whatever has keyboard focus, and stays the scene's until released. When the protocol or `InputKey` order changes,
increment the version and rebuild editor and worker together. The backend key table is checked against the enum with
identified rows and `RowsInIdOrder`.

## Menus

Menus, lists and grids are in [UI](Ui.md), which reads this input itself.
