# Gameplay helpers

## Gameplay frameworks

Headless helpers (EngineHeadless). The gameplay frameworks are header-only and templated on the game's own enum;
Engine never names game types. Input navigation is a compiled borrowed view.

| Header | Use |
| --- | --- |
| `Gameplay/StatSheet.h` | `StatSheet<Id, Rules>`: value = (`Rules::Base` + ΣAdd + bonus) × ΠMultiply, then `Rules::Finalize` (clamps). `StatModifierOf<Id>`, `ModOp` |
| `Gameplay/TimedEffectSet.h` | Re-added timed effects add their time and gain a stack (up to `maxStacks`), persistent ones are separate copies; `RefreshTimed` refills active timers with an optional excluded ID; `Update` reports expiry; `FirstTimed` |
| `Progress/TieredProgress.h` | Tiered goals (achievements): tier / seen / carried progress, saved as `<key>.tier/seen/progress` with clamps |
| `Fx/CollectFlights.h` | Pickup-to-counter flights; `Pending(kind)` keeps credit off a counter until arrival |
| `Utilities/WeightedPick.h` | `WeightedIndex(weights, rng)`: seed-stable `std::discrete_distribution` pick |
| `Utilities/EnumTable.h` | `static_assert(RowsInIdOrder(rows))`: rows in enum order, one per value (`Id::Count` last) |
| `Utilities/ChoiceList.h` | `StoredChoice` / `StepChoice` for settings lists (control scheme, FPS limit) |
| `Input/InputNavigation.hpp` | Borrowed semantic queries over `DeviceInput` (Confirm, Back, axes, tabs, press point) |

```cpp
enum class Attribute { Speed, Luck, Count };
struct AttributeRules {
    static float Base(Attribute a) { return a == Attribute::Speed ? 10.0f : 0.0f; }
    static float Finalize(Attribute a, float v) { return a == Attribute::Luck ? std::clamp(v, 0.0f, 1.0f) : v; }
};
using Attributes = StatSheet<Attribute, AttributeRules>;
```

## Modifier ve süreli etki örneği

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
    effects.AddTimed(Status::Haste, 1.0f, 3); // 3 saniye kalır, 2 stack
    const bool expired = effects.Update(3.0f);
    (void)expired;
}
```

`Apply` modifier biriktirir; her frame aynı listeyi tekrar uygulamayın. Baştan hesaplamak için önce
`Clear()` çağırın. Etkinin stat'a nasıl dönüştüğü ve stack başına gücü oyunun kuralıdır.
Pozitif süre ve geçerli delta verin; bu basit konteynerlerde genel input doğrulaması varsaymayın.

## Kademeli ilerleme

`TieredProgress<N>` oyun hedeflerinin tier/seen/progress verisini tutar. Oyun, geçerli indeksleri,
hedef eşiklerini, hangi olayların sayıldığını ve tier üst sınırını yönetir.
`Load(save, keyOf, tierCountOf)` kayıtları sınırlar; `Store(save, keyOf, storesProgress)`
yalnız Persistence değerlerini değiştirir, diske kaydetmez. Ardından oyunun kayıt noktasında
`Persistence::Save()` çağrılır. Kalıcı key'leri enum sırası yerine sabit metinlerden üretin.

```cpp
TieredProgress<1> goals;
goals.SetProgress(0, 12.0f);
goals.CompleteTier(0); // oyun önce hedefin tamamlandığını doğrular
const int notifications = goals.UnseenCount();
goals.MarkSeen();
```

Bu örnek için ayrıca `Progress/TieredProgress.h` include edilir. Kayıt, RNG ve UI politikaları
bu yardımcıların dışında kalır. Koleksiyon gezinmesi [Input](Input.md), sayaç animasyonu
[Themes and UI](Graphics/ThemesAndUi.md) altında anlatılır.
