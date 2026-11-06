#include <cstdint>
#include <memory>
#include "Runtime/EntryPoint.h"
#include "Runtime/Game.h"
#include "Scenes/PlayScene.h"

namespace {
constexpr std::uint64_t Seed = 1;

// Title, canvas and font come from Game/Game.settings.json.
GameSettings Settings() {
    GameSettings settings;
    return settings;
}

class ArenaGame final : public Game {
public:
    ArenaGame()
        : Game(Settings())
        , _play(Services(), Seed) {}

private:
    void Start() override {
        RegisterScene(_play);
        SetScene(_play);
    }
    void Update(float realSeconds) override {
        Game::Update(realSeconds);
        if (_play.QuitRequested()) RequestQuit();
    }

    PlayScene _play;
};
} // namespace

std::unique_ptr<Game> CreateGame() { return std::make_unique<ArenaGame>(); }
