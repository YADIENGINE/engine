// [recipe:PopupEffects]
#include "AgentDocs/Recipes/Code/PopupEffects.h"
#include <utility>

void RecipePopupEffects::Hit(Float2 position, NumberPopupValue value, RecipeHitGroup group, int target) {
    _numbers.Emit(position, value, NumberPopupPreset::Burst, {.mergeKey = NumberPopupMergeKey::Of(group, target)});
}

void RecipePopupEffects::Heal(Float2 position, NumberPopupValue value) {
    _numbers.Emit(position, value, NumberPopupPreset::Float, {.color = Color4{90, 255, 140, 255}});
}

void RecipePopupEffects::Reward(Float2 position, std::string label) {
    _numbers.EmitLabel(position, std::move(label), NumberPopupPreset::Heavy, {.color = Color4{255, 200, 60, 255}});
}

// Plain Popup: any drawing; PopupText is the ready label.
void RecipePopupEffects::Notice(Float2 position, std::string label) {
    _popups.Emit(position, PopupText(std::move(label), {.size = 28.0f}), PopupPreset::Pop);
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
