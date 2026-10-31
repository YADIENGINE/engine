#include <memory>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/TextStyle.h"
#include "Input/InputNavigation.h"
#include "Runtime/EntryPoint.h"
#include "Runtime/Game.h"

namespace {
constexpr int CanvasWidth = 1280;
constexpr int CanvasHeight = 720;
constexpr int TitleSize = 48;
constexpr int HintSize = 24;
constexpr int LineGap = 64;
constexpr Color4 TitleColor{245, 245, 245, 255};
constexpr Color4 HintColor{130, 130, 130, 255};

GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = ENGINE_NAME "Starter";
    settings.window.title = ENGINE_NAME " starter";
    settings.canvasWidth = CanvasWidth;
    settings.canvasHeight = CanvasHeight;
    settings.persistentOptions = false;
    settings.font = "Fonts/Starter.ttf";
    settings.assets = {settings.font};
    return settings;
}

// engine_add_game builds this in place of the project's CreateGame while Game/ has no sources.
class StarterGame final : public Game {
public:
    StarterGame()
        : Game(Settings()) {}

private:
    void HandleInput(const DeviceInput& input) override {
        if (InputNavigation(input).Back()) RequestQuit();
    }
    void Draw(Painter& paint) const override {
        constexpr int CenterX = CanvasWidth / 2;
        constexpr int CenterY = CanvasHeight / 2;
        paint.Text("Game/ has no sources yet", {CenterX, CenterY - LineGap}, CenteredText(TitleSize, TitleColor));
        paint.Text("Add Game/CreateGame.cpp: Engine/Docs/GettingStarted.md", {CenterX, CenterY},
                   CenteredText(HintSize, HintColor));
        paint.Text("Esc quits", {CenterX, CenterY + LineGap}, CenteredText(HintSize, HintColor));
    }
};
} // namespace

std::unique_ptr<Game> CreateGame() { return std::make_unique<StarterGame>(); }
