// [recipe:PopupEffects]
#include "AgentDocs/Recipes/Code/PopupEffects.h"
#include <utility>
#include "Graphics/Paint/Painter.h"

void RecipePopupEffects::Hit(Float2 position, NumberPopupValue value, NumberPopupMergeKey group) {
    _numbers.Emit(position, value, _impact, 1.0f, group);
}

void RecipePopupEffects::Reward(Float2 position, std::string label) {
    auto style = Popup::Preset(PopupPreset::Pop);
    style.enter.scale = {0.5f, 1.3f};
    style.motion.velocity = {0.0f, -80.0f};
    _popups.Emit(
        position,
        [label = std::move(label)](Painter& paint, const PopupFrame&) {
            paint.Text(label, {}, {.size = 28.0f, .align = TextAlign::Center});
        },
        style);
}

void RecipePopupEffects::SetQuality(const FxQuality& quality) { _numbers.SetQuality(quality); }
void RecipePopupEffects::Update(float deltaSeconds) {
    _popups.Update(deltaSeconds);
    _numbers.Update(deltaSeconds);
}
void RecipePopupEffects::Draw(Painter& paint) const {
    _numbers.Draw(paint);
    _popups.Draw(paint);
}
void RecipePopupEffects::Clear() {
    _popups.Clear();
    _numbers.Clear();
}
// [/recipe]
