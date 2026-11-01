Navigate and animate a two-button themed menu.

APIs: Core/Graphics/Ui/UiDraw.h, Core/Graphics/Ui/UiButton.h, Core/Input/InputNavigation.h, Core/Menus/MenuSelection.h.

Call Navigate from scene input, act on Selected when it returns true, and Update with the menu delta. Configure GameSettings::font before drawing labeled widgets.

<!-- recipe:ThemedMenu-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:ThemedMenu -->
```cpp
#include "AgentDocs/Recipes/Code/ThemedMenu.h"
#include "Graphics/Ui/UiDraw.h"
#include "Input/InputNavigation.h"

namespace {
constexpr Color4 Accent = Rgb(0x60c0ff);
constexpr FloatRect PanelArea{80.0f, 80.0f, 320.0f, 240.0f};
constexpr FloatRect PlayArea{100.0f, 100.0f, 280.0f, 64.0f};
constexpr FloatRect QuitArea{100.0f, 200.0f, 280.0f, 64.0f};
constexpr float LabelSize = 24.0f;
constexpr auto PlayIndex = static_cast<std::size_t>(RecipeMenuItem::Play);
constexpr auto QuitIndex = static_cast<std::size_t>(RecipeMenuItem::Quit);
} // namespace

bool RecipeMenu::Navigate(const DeviceInput& input) {
    const InputNavigation navigation(input);
    if (navigation.Vertical() > 0) _selection.SelectNext();
    if (navigation.Vertical() < 0) _selection.SelectPrevious();
    if (!navigation.Confirm()) return false;
    _buttons[static_cast<std::size_t>(Selected())].Press();
    return true;
}
void RecipeMenu::Update(float deltaSeconds) {
    _buttons[PlayIndex].Update(Selected() == RecipeMenuItem::Play, deltaSeconds);
    _buttons[QuitIndex].Update(Selected() == RecipeMenuItem::Quit, deltaSeconds);
}
void RecipeMenu::Draw(Painter& paint) const {
    DrawUiPanel(paint, PanelArea, Accent);
    DrawUiButton(paint, PlayArea, "PLAY", LabelSize, _buttons[PlayIndex].Look(Accent));
    DrawUiButton(paint, QuitArea, "QUIT", LabelSize, _buttons[QuitIndex].Look(Accent));
}
```
<!-- /recipe -->

Pitfalls: InputNavigation borrows its snapshot; never store it. Forward only normal scene input; cancelled snapshots are cancellation barriers.

Concept: `Docs/Input.md#menu-selection-and-viewport`.
