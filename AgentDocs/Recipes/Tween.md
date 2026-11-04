# Tween recipe

Move values in a menu or a view: a title that pops in, a cursor that follows, coins in a list that grows and shrinks, with one `Update` for each group.

APIs: Core/Animation/Tween.h, Core/Animation/TweenSet.h, Core/Animation/Curve.h. Concepts: [Animation](../../Docs/Animation.md).

<!-- recipe:TweenMenu-header -->
```cpp
#pragma once
#include <cstddef>
#include <vector>
#include "Animation/Tween.h"
#include "Animation/TweenSet.h"
#include "Utilities/Geometry/Float2.h"

class Painter;

enum class RecipeMenuPart { Title, Glow };

// A view on ordinary time: a coin counts as a value with its own tweens, so it needs no pointer into a list that moves.
struct RecipeCoin {
    Float2 at;
    SecondsTween<float> scale{0.0f};
    SecondsTween<float> alpha{1.0f};
};

class RecipeTweenMenu {
public:
    void Open();
    void MoveCursor(Float2 to);
    void AddCoin(Float2 at);
    void CollectCoin(std::size_t index);
    void Update(float deltaSeconds);
    void Draw(Painter& paint) const;

    [[nodiscard]] float TitleScale() const { return _parts[RecipeMenuPart::Title]; }
    [[nodiscard]] int TitleBeats() const { return _titleBeats; }
    [[nodiscard]] std::size_t Coins() const { return _coins.size(); }

private:
    SecondsTweenSet<RecipeMenuPart> _parts{1.0f}; // Values that need no field of their own
    SecondsTween<Float2> _cursor;                 // A value with a field: reads like a Float2
    std::vector<RecipeCoin> _coins;
    int _titleBeats = 0;
};
```
<!-- /recipe -->

<!-- recipe:TweenMenu -->
```cpp
#include "AgentDocs/Recipes/Code/TweenMenu.h"
#include <algorithm>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Color4.h"

namespace {
constexpr Float2 TitleAt{400.0f, 120.0f};
constexpr Color4 Gold{255, 210, 90, 255};
} // namespace

void RecipeTweenMenu::Open() {
    _parts.Set(RecipeMenuPart::Title, 0.0f);
    _parts.To(RecipeMenuPart::Title, 1.2f, 0.25f, Curve::OutBack).Then(1.0f, 0.15f, Curve::InOutQuad);
    // Loop never ends, so this one needs stopping when the menu closes.
    _parts.To(RecipeMenuPart::Glow, 1.3f, 0.6f, Curve::InOutSine).Then(1.0f, 0.6f, Curve::InOutSine).Loop();
}
void RecipeTweenMenu::MoveCursor(Float2 to) {
    _cursor.To(to, 0.2f, Curve::OutCubic); // Continues from where it is, even in the middle of the last move
}
void RecipeTweenMenu::AddCoin(Float2 at) {
    _coins.push_back({at});
    _coins.back().scale.To(1.0f, 0.3f, Curve::OutBounce);
}
void RecipeTweenMenu::CollectCoin(std::size_t index) {
    _coins[index].scale.To(1.5f, 0.2f, Curve::OutQuad);
    _coins[index].alpha.To(0.0f, 0.2f, Curve::InQuad);
}
void RecipeTweenMenu::Update(float deltaSeconds) {
    _parts.Update(deltaSeconds, [&](RecipeMenuPart finished) {
        if (finished == RecipeMenuPart::Title) ++_titleBeats; // In a game: a sound. Nothing is stored, so nothing dangles
    });
    _cursor.Update(deltaSeconds);
    UpdateAll(deltaSeconds, _coins, &RecipeCoin::scale, &RecipeCoin::alpha);
    std::erase_if(_coins, [](const RecipeCoin& coin) { return coin.alpha.JustFinished(); });
}
void RecipeTweenMenu::Draw(Painter& paint) const {
    paint.Circle(TitleAt, 40.0f * _parts[RecipeMenuPart::Title] * _parts[RecipeMenuPart::Glow], Fill(Gold));
    paint.Circle(_cursor, 6.0f, Fill(Gold));
    for (const auto& coin : _coins)
        paint.Circle(coin.at, 12.0f * coin.scale, Fill(WithAlpha(Gold, coin.alpha)));
}
```
<!-- /recipe -->

Checks:

- A tween is the value, so it goes where the value would go. `paint.Circle(_cursor, ...)` and `12.0f * coin.scale` read it directly; use `.Value()` where C++ cannot work out the conversion.
- Every tween needs one `Update` a frame. `UpdateAll` covers fields and lists. A tween that is never updated stays where it is, with no error.
- `auto x = coin.scale` copies the tween, not the number. Write `float x = coin.scale`.
- A list can grow and shrink freely: `UpdateAll` walks it when called and keeps nothing.
- For rules, use `StepTween` with `Step()` and `State`: see the [animator recipe](Animator.md).
