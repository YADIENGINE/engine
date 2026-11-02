#pragma once
#include <string>
#include "Modules/NumberPopup/NumberPopup.h"
#include "Modules/Popup/Popup.h"

class Painter;

class RecipePopupEffects {
public:
    void Hit(Float2 position, NumberPopupValue value, NumberPopupMergeKey group = {});
    void Reward(Float2 position, std::string label);
    void SetQuality(const FxQuality& quality);
    void Update(float deltaSeconds);
    void Draw(Painter& paint) const;
    void Clear();

private:
    Popup _popups;
    NumberPopup _numbers;
    NumberPopupStyle _impact = NumberPopup::Preset(NumberPopupPreset::Burst);
};
