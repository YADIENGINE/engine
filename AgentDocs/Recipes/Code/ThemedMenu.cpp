// [recipe:ThemedMenu]
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
// [/recipe]
