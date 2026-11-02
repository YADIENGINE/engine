# Popup

Opt-in animated drawing callbacks with an explicit lifetime, local coordinate system and opaque handles.
Link `EnginePopup`; include `Modules/Popup/Popup.h`. Dependencies: `EngineHeadless` for Painter, math and easing.
No core system instantiates this module or advances it automatically.

## Usage

Keep a `Popup` as a View/Scene member. Emit on events, call `Update(chosenDelta)` once per frame, then `Draw(paint)`
at the desired layer. Call `Clear()` on a scene/run reset. The owner chooses game time or real time.

```cpp
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/TextStyle.h"
#include "Modules/Popup/Popup.h"

Popup popups;
auto style = Popup::Preset(PopupPreset::Pop);
style.motion.velocity = {0.0f, -80.0f};
auto handle = popups.Emit({400.0f, 300.0f}, [](Painter& paint, const PopupFrame&) {
    paint.Text("BONUS", {}, {.size = 28.0f, .align = TextAlign::Center});
}, style);
// In the owner's Update and Draw methods:
popups.Update(dt);
popups.Draw(paint);
```

The compiling consumer recipe is [PopupEffects](../../../AgentDocs/Recipes/PopupEffects.md).

## Public API

| Operation | Contract |
| --- | --- |
| `Popup(settings = {})` | Capacity 128 and ReplaceOldest by default. Capacity zero disables emission. |
| `Preset(preset)` | Returns an editable style: Fade, Pop, Float or Bounce. No theme access. |
| `Duration(style)` | Validates the style and returns enter + hold + exit seconds, excluding delay. |
| `Emit(position, draw, style = {})` | Owns the callback and a copy of the style. Returns a handle; empty on capacity rejection. |
| `Update(deltaSeconds)` | Advances lifetimes. Nonpositive/nonfinite deltas do nothing. Large steps expire entries directly. |
| `Draw(paint) const` | Samples the current pose, applies transform/opacity scopes, then invokes the callback. |
| `SetAnchor(handle, position)` | Moves the reference position without resetting age or motion. |
| `Pulse(handle, pulse = {})` | Replaces the decaying scale/rotation overlay; does not extend lifetime. |
| `Extend(handle, seconds, maxLifetimeSeconds)` | Adds hold time before Exit, up to the total active lifetime cap. |
| `Dismiss(handle)` | Starts Exit from the current pose. Already exiting entries keep their deadline. |
| `Cancel(handle)` | Immediately removes an entry. |
| `Clear()` | Removes all entries and releases callbacks. Invalidates every issued handle. |
| `IsAlive(handle)` | Whether this collection still contains that handle. |
| `Frame(handle)` | Optional owned pose/timing snapshot; empty for expired, cancelled or foreign handles. |
| `ActiveCount()` | Number of stored entries, including delayed entries. |

Handle operations return false for foreign/stale handles. `Extend` also returns false during Exit; reaching its cap
is a successful no-op. Invalid arguments still throw even when the handle is stale. A nonempty handle identifies a
successful emission, not continued liveness. IDs are not reused after Clear; handles do not keep the system alive.

## Animation and drawing

`PopupStyle` contains Enter, Hold and Exit. A transition has seconds, offset, two-axis scale, clockwise-radian
rotation, opacity and an easing function pointer. Enter interpolates its endpoint to the normal pose; Exit goes
from normal to its endpoint. Zero-duration transitions are skipped; total active duration must be positive.
`delaySeconds` precedes the active lifetime and never invokes drawing. Dismissing a delayed entry cancels it.

`PopupMotion` independently adds velocity, constant acceleration, base rotation, spin and an exponentially damped
wobble. Motion is evaluated analytically from active age; update subdivision does not change its trajectory.
`SetAnchor` translates that trajectory. Dismiss keeps the sampled pose and continues the existing movement during
the configured exit duration. A zero-duration exit cancels immediately.

`PopupPulse` is an additional fractional scale and rotation that decays over `seconds`. A delayed pulse starts with
the entry. Retriggering replaces this overlay, so repeated calls do not build an unbounded scale.

Callbacks draw around local `(0, 0)`. Their Painter already has the sampled translation, rotation, scale and opacity.
Do not apply `PopupFrame`'s pose/opacity a second time. Frame also exposes active age, normalized lifetime progress,
phase and phase progress. Extending a lifetime changes the denominator of lifetime progress, not age or movement.
Opacity is clamped to 0..1; easing overshoot is retained for motion/scale, with negative resulting scale clamped to
zero. Fully transparent/zero-scale entries are not drawn.

Coordinates inherit the caller's Painter space. Apply a world camera outside Draw for world popups, or draw directly
on the logical canvas for HUD popups. Lower `layer` values draw first, with stable emission order inside a layer.
Capacity eviction always removes the oldest emission, independently of its layer.

## Ownership and failures

The collection is noncopyable/nonmovable and main-thread only. It stores no Painter, font, texture or scene reference.
Value captures belong to the entry; references and captured resource handles must remain valid until removal.
Drawing callbacks and easing functions must not reenter or mutate the collection. Draw rejects mutation/reentry
with `logic_error` and restores Painter scopes if a callback throws. Easing functions must be pure and finite,
with endpoints 0 at input 0 and 1 at input 1.
Invalid styles/positions, empty callbacks and invalid enum values throw `invalid_argument`; callback exceptions
propagate. Draw only advances layout/render caches owned by the drawing code, never the popup's timeline.
