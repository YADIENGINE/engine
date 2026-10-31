#include "Editor/Preview/PreviewGame.h"

PreviewGame::PreviewGame(const GameSettings& settings, const PreviewRegistry& registry, ControlChannel& channel,
                         const std::string& session)
    : Game(settings)
    , _preview(*this, settings, registry, channel, session, Text(), Audio(),
               {[this] { RequestQuit(); },
                [this] { Time().Clear(); },
                [this](PreviewScene& scene) { SetScene(scene); },
                {}}) {
    RegisterScene(_preview.Scene());
}
PreviewGame::~PreviewGame() { _preview.Scene().ShutdownNoThrow(); }
void PreviewGame::Start() { SetScene(_preview.Scene()); }
void PreviewGame::PrepareFrame() { _preview.PrepareFrame(); }
DeviceInput PreviewGame::ReadInput() { return _preview.ReadInput(); }
void PreviewGame::HandleInput(const DeviceInput& input) {
    Game::HandleInput(input);
    _preview.Capture();
}
float PreviewGame::FrameSeconds(float seconds) { return _preview.Advance(seconds); }
double PreviewGame::DrawSeconds() const { return _preview.Scene().Seconds(); }
void PreviewGame::CanvasReady(const Texture2D& texture) const { _preview.CanvasReady(texture); }
void PreviewGame::Stop() { _preview.Stop(); }
