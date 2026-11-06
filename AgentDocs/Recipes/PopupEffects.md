Animate arbitrary drawing callbacks and emit numeric feedback with configurable local juice.

APIs: Core/Modules/Popup/Popup.h, Core/Modules/NumberPopup/NumberPopup.h.

Link EngineNumberPopup (which brings EnginePopup), and own one in a View/Scene. Emit on events; call SetQuality with
the player's effects setting, Update with the chosen delta, Draw with the game's font, and Clear on reset. Equal merge
keys accumulate compatible hits within their window. NumberPopup alone covers numbers and text labels; add Popup only
for custom drawing. Which text effect to pick: `Docs/Graphics/Effects.md#choosing-a-text-effect`.

<!-- recipe:PopupEffects -->
```cpp
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
```
<!-- /recipe -->

Pitfalls: custom callbacks draw around local zero, and `paint.Text` positions the top of the first line, so use
`PopupText`/`DrawPopupText` for text. Popup already applies the animation transform and opacity. Capture labels by
value. Keep borrowed resources alive. Place Draw inside or outside the world camera according to the desired space.

Concepts: `Core/Modules/Popup/README.md`, `Core/Modules/NumberPopup/README.md`.
