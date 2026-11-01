// [recipe:ThemedMenu-header]
#pragma once
#include <array>
#include "Graphics/Ui/UiButton.h"
#include "Menus/MenuSelection.h"

struct DeviceInput;
class Painter;
enum class RecipeMenuItem { Play, Quit };

class RecipeMenu {
public:
    bool Navigate(const DeviceInput& input);
    void Update(float deltaSeconds);
    void Draw(Painter& paint) const;
    [[nodiscard]] RecipeMenuItem Selected() const { return _selection.Selected(); }

private:
    MenuSelection<RecipeMenuItem, RecipeMenuItem::Play, RecipeMenuItem::Quit> _selection;
    std::array<UiButton, 2> _buttons;
};
// [/recipe]
