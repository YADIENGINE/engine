// [recipe:TweenMenu]
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
// [/recipe]
