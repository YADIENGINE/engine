#include "MyGame.h"
#include "Input/InputFrame.h"

namespace {
GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = "MyProject";
    settings.window.title = "My first game";
    settings.canvasWidth = 1280;
    settings.canvasHeight = 720;
    return settings;
}
} // namespace

MyGame::MyGame()
    : Game(Settings()) {
    RegisterScene(_scene);
}
void MyGame::Start() { SetScene(_scene); }
void MyGame::HandleInput(const DeviceInput& input) {
    if (InputFrame(input).Back())
        RequestQuit();
    else
        Game::HandleInput(input);
}
