#include <raylib.h>
#include <memory>
#include "Input/InputNavigation.hpp"
#include "Runtime/EntryPoint.h"
#include "Runtime/Game.h"

namespace {
constexpr int CanvasWidth = 1280;
constexpr int CanvasHeight = 720;
constexpr int TitleSize = 48;
constexpr int HintSize = 24;
constexpr int LineGap = 64;

GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = ENGINE_NAME "Starter";
    settings.window.title = ENGINE_NAME " starter";
    settings.canvasWidth = CanvasWidth;
    settings.canvasHeight = CanvasHeight;
    settings.persistentOptions = false;
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
    void Draw() const override {
        constexpr int CenterX = CanvasWidth / 2;
        constexpr int CenterY = CanvasHeight / 2;
        Text().DrawCentered("Game/ has no sources yet", CenterX, CenterY - LineGap, TitleSize, RAYWHITE);
        Text().DrawCentered("Add Game/CreateGame.cpp: Engine/Docs/GettingStarted.md", CenterX, CenterY, HintSize, GRAY);
        Text().DrawCentered("Esc quits", CenterX, CenterY + LineGap, HintSize, GRAY);
    }
};
} // namespace

std::unique_ptr<Game> CreateGame() { return std::make_unique<StarterGame>(); }
