# Gameplay helpers

## Gameplay frameworks

Headless helpers (EngineHeadless). The gameplay frameworks are header-only and templated on the game's own enum; Engine
never names game types. Input navigation is a compiled borrowed view.

| Header | Use |
| --- | --- |
| `Gameplay/StatSheet.h` | `StatSheet<Id, Rules>`: value = (`Rules::Base` + ΣAdd + bonus) × ΠMultiply, then `Rules::Finalize` (clamps). `StatModifierOf<Id>`, `ModOp` |
| `Gameplay/TimedEffectSet.h` | Re-added timed effects add their time and gain a stack (up to `maxStacks`), persistent ones are separate copies; `RefreshTimed` refills active timers with an optional excluded ID; `Update` reports expiry; `FirstTimed` |
| `Progress/TieredProgress.h` | Tiered goals (achievements): tier / seen / carried progress, saved as `<key>.tier/seen/progress` with clamps |
| `Fx/CollectFlights.h` | Pickup-to-counter flights; `Pending(kind)` keeps credit off a counter until arrival |
| `Utilities/WeightedPick.h` | `WeightedIndex(weights, rng)`: seed-stable `std::discrete_distribution` pick |
| `Utilities/EnumTable.h` | `static_assert(RowsInIdOrder(rows))`: rows in enum order, one per value (`Id::Count` last) |
| `Utilities/ChoiceList.h` | `StoredChoice` / `StepChoice` for settings lists (control scheme, FPS limit) |
| `Input/InputNavigation.h` | Borrowed semantic queries over `DeviceInput` (Confirm, Back, axes, tabs, press point) |

```cpp
enum class Attribute { Speed, Luck, Count };
struct AttributeRules {
    static float Base(Attribute a) { return a == Attribute::Speed ? 10.0f : 0.0f; }
    static float Finalize(Attribute a, float v) { return a == Attribute::Luck ? std::clamp(v, 0.0f, 1.0f) : v; }
};
using Attributes = StatSheet<Attribute, AttributeRules>;
```

## Modifier and timed effect example

```cpp
#include <array>
#include "Gameplay/StatSheet.h"
#include "Gameplay/TimedEffectSet.h"

enum class Attribute { Speed, Luck, Count };
struct AttributeRules {
    static float Base(Attribute id) { return id == Attribute::Speed ? 10.0f : 0.0f; }
    static float Finalize(Attribute, float value) { return value; }
};
enum class Status { Haste };
using Attributes = StatSheet<Attribute, AttributeRules>;

void Example() {
    Attributes attributes;
    const std::array modifiers{StatModifierOf<Attribute>{Attribute::Speed, ModOp::Multiply, 1.5f}};
    attributes.Apply(modifiers);
    const float speed = attributes.Get(Attribute::Speed); // 15
    (void)speed;

    TimedEffectSet<Status> effects;
    effects.AddTimed(Status::Haste, 2.0f, 3);
    effects.AddTimed(Status::Haste, 1.0f, 3); // 3 seconds remain, 2 stacks
    const bool expired = effects.Update(3.0f);
    (void)expired;
}
```

`Apply` accumulates modifiers; do not reapply the same list every frame. To recalculate from scratch, call `Clear()`
first. How effects translate into stats and their strength per stack are game rules. Supply positive durations and valid
deltas; do not assume general input validation in these simple containers.

## Tiered progress

`TieredProgress<N>` holds tier/seen/progress data for game goals. The game manages valid indices, goal thresholds, which
events count, and the tier limit. `Load(save, keyOf, tierCountOf)` clamps saved values; `Store(save, keyOf,
storesProgress)` only changes Persistence values, without saving to disk. Call `Persistence::Save()` afterwards at the
game's save point. Derive persistent keys from stable strings instead of enum order.

```cpp
TieredProgress<1> goals;
goals.SetProgress(0, 12.0f);
goals.CompleteTier(0); // the game checks goal completion first
const int notifications = goals.UnseenCount();
goals.MarkSeen();
```

Also include `Progress/TieredProgress.h` for this example. Save, RNG, and UI policies stay outside these helpers.
Collection navigation is described in [Input](Input.md), and counter animation in [Themes and
UI](Graphics/ThemesAndUi.md).
