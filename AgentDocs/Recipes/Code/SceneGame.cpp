// [recipe:SceneGame]
#include "AgentDocs/Recipes/Code/SceneGame.h"
#include <memory>
#include "Graphics/Paint/Painter.h"
#include "Input/InputNavigation.h"
#include "Runtime/EntryPoint.h"

namespace {
constexpr float CloseSeconds = 0.2f;
constexpr float HoldSeconds = 0.0f;
constexpr float OpenSeconds = 0.2f;
constexpr Color4 CurtainColor = Rgb(0x000000);
} // namespace

RecipeGame::RecipeGame(const GameSettings& settings)
    : Game(settings)
    , _transition(CloseSeconds, HoldSeconds, OpenSeconds) {}
void RecipeGame::Start() {
    RegisterScene(_first);
    RegisterScene(_second);
    SetScene(_first);
}
void RecipeGame::HandleInput(const DeviceInput& input) {
    if (_transition.Active()) return;
    if (InputNavigation(input).Confirm()) {
        CancelInput();
        _transition.Start();
    } else
        Game::HandleInput(input);
}
void RecipeGame::Update(float realSeconds) {
    Game::Update(realSeconds);
    if (_transition.Update(realSeconds)) SetScene(_second);
}
void RecipeGame::Draw(Painter& paint) const {
    bool opening = false;
    const float cover = _transition.Cover(opening);
    paint.Rect(paint.Bounds(), Fill(WithAlpha(CurtainColor, cover)));
}
std::unique_ptr<Game> CreateGame() {
    GameSettings settings;
    settings.applicationId = "MyProject";
    settings.window.title = "MyProject";
    return std::make_unique<RecipeGame>(settings);
}
// [/recipe]
