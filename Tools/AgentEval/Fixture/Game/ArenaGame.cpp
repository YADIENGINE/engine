#include <cstdint>
#include <memory>
#include "Input/InputNavigation.h"
#include "Runtime/EntryPoint.h"
#include "Runtime/Game.h"
#include "Scenes/PlayScene.h"

namespace {
constexpr int CanvasWidth = 1280;
constexpr int CanvasHeight = 720;
constexpr std::uint64_t Seed = 1;

GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = "Arena";
    settings.window.title = "Arena";
    settings.canvasWidth = CanvasWidth;
    settings.canvasHeight = CanvasHeight;
    settings.font = "Fonts/Arena.ttf";
    settings.assets = {settings.font};
    return settings;
}

class ArenaGame final : public Game {
public:
    ArenaGame()
        : Game(Settings())
        , _play(Time(), Seed) {}

private:
    void Start() override {
        RegisterScene(_play);
        SetScene(_play);
    }
    void HandleInput(const DeviceInput& input) override {
        if (InputNavigation(input).Back()) {
            RequestQuit();
            return;
        }
        Game::HandleInput(input);
    }

    PlayScene _play;
};
} // namespace

std::unique_ptr<Game> CreateGame() { return std::make_unique<ArenaGame>(); }
