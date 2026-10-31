# Input and navigation

## Public API and ownership

Input is engine runtime infrastructure. Supported public headers:

| Header | Contract |
| --- | --- |
| `Input/Input.h` | `DeviceInput`, `InputKey`, `InputTouch`; raylib-free snapshot |
| `Input/InputNavigation.h` | `InputNavigation` reading a snapshot by reference |
| `Input/DragPointer.h` | Free dragging and tap on release |
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

`DeviceInput` carries key pressed/down states, mouse, wheel, and identified touch samples. `ShiftDown()` is computed
from keys. `mousePosition` is the current position; `mousePress` is an eligible mouse press; `mouseHover` is populated
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

`CollectionPointer` and `HeaderGridNavigation` live in EngineHeadless. They contain no rendering or content policy.
Create a pointer helper with the grid bounds and row-drag threshold, then call `Update(DeviceInput)` once per input
frame. Its result contains a release `tap`, eligible `hover`, and signed `rows` (positive moves down the collection).
Touch movement emits rows while held. Crossing the movement threshold latches drag detection until release, including
when the contact returns to its origin. The first touch ID owns the gesture; later contacts cannot inherit it. Touch
also suppresses synthetic mouse events until all contacts and mouse buttons are quiet. Mouse taps activate on release;
wheel input applies only over the grid. Call `Cancel()` for a local gesture cancellation, then continue supplying input
frames on return so the cancelled contact can drain. Keyboard bindings, audio and confirmation remain in Game.

`HeaderGridNavigation(count, {headerRowSize, ...})` owns `GridMenuSelection` plus focus across header rows, grid,
empty-state action and footer. `Move(dx, dy, columns)` reports a focus/selection change. During filtering, use
`SetItemCount` followed by `SelectIndex` to retain header focus; preserve stable content identity in the caller. `Focus`
and `HeaderIndex` support hover/click adapters without coupling Engine to category enums. `ResetHeaders` returns every
header cursor to its first item without moving focus. Every header row must have at least one item (asserted).



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

Editor protocol v4 carries snapshot keys, mouse fields, combined contact lists, and identified touches; an empty touch
position preserves identity outside the canvas. Duplicate touch IDs and capacity overflow are rejected. Worker
accumulation preserves press/release flags, accumulates `pressPoints`, and takes down/held/touch state from the latest
snapshot. Cancellation clears old actions and is delivered as a separate barrier before new actions. This transport
merges snapshots; it is not an ordered log of all physical events. When the protocol or `InputKey` order changes,
increment the version and rebuild editor and worker together. The backend key table is checked against the enum with
identified rows and `RowsInIdOrder`.

## Menu selection and viewport

`Graphics/Draw/GridViewport.h` complements grid selection with reusable scrolling and tile geometry. Each View owns a
value member initialized with `GridViewportSettings`: canvas `bounds`, `columns`, `visibleRows`,
`tileWidth`/`tileHeight`, independent `gapX`/`gapY`, and `scrollSpeed`. Bounds, dimensions, columns and visible rows
must be positive; gaps and finite speed must be nonnegative. Zero speed snaps immediately. The component draws nothing
and does not manage selection, focus, item IDs or input.

```cpp
GridViewport viewport({.bounds = gridBounds, .columns = 4, .visibleRows = 3,
                       .tileWidth = 120, .tileHeight = 100, .gapX = 12, .gapY = 16,
                       .scrollSpeed = 10.0f});
viewport.Reset(itemCount, selectedIndex); // scene entry or a content/filter reset
viewport.Update(itemCount, selectedIndex, deltaSeconds);
const Rect tile = viewport.TileArea(index);
const auto hit = viewport.TileAt(pointer, itemCount);
```

`Reset` bottom-aligns the selected row where possible. `Update` retains scroll while selection is visible and otherwise
interpolates toward its visible range, using `min(1, dt * scrollSpeed)`. Invalid update deltas skip animation; count
shrinkage still clamps scroll and an empty count resets it to zero. Negative counts are empty and selection is clamped
for calculations without changing the caller's selection. `TileArea` rounds animated vertical positions with
`std::lround`; negative indices return an empty rectangle. `TileAt` rejects points outside bounds, gaps and missing
items. It uses the supplied current count, even when input changed content before the next View update.
`RowCount(count)` and `ScrollRows()` allow Game to draw its own scrollbar and entrance animations. Keep drawing order,
expanded scissor margins, colors, and filter/category reset decisions in the View.

`Menus/GridMenuSelection.h` owns a grid's selected index and `Grid`/`Footer` focus. Construct it with an item count and
call `Move(dx, dy, columns)`. Horizontal movement follows the linear item order; vertical movement clamps incomplete
rows. `GridMenuMove::ExitAbove` lets the game focus controls above the grid. Down from the last row focuses the footer;
up restores the selected item. `SetItemCount` clamps selection when results change. An empty grid selects `-1` and only
allows footer focus. Invalid column counts produce no movement. Item IDs, filtering, ordering, and additional focus
regions belong to the game.



`MenuSelection` and `MenuInput` are smaller options for one-dimensional menus. Input changes selection; the View owns
scrolling/hover/animation. Use the same viewport geometry for hit testing as for drawing.
