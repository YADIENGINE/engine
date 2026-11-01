#include "Editor/Worker/WorkerGame.h"

WorkerGame::WorkerGame(const GameSettings& settings, const SceneRegistry& registry, ControlChannel& channel,
                       const std::string& session)
    : Game(settings)
    , _session(*this, settings, registry, channel, session, HasFont() ? &Font() : nullptr, Audio(),
               {[this] { RequestQuit(); },
                [this] { Time().Clear(); },
                [this](WorkerScene& scene) { SetScene(scene); },
                {}}) {
    RegisterScene(_session.Scene());
}
WorkerGame::~WorkerGame() { _session.Scene().ShutdownNoThrow(); }
void WorkerGame::Start() { SetScene(_session.Scene()); }
void WorkerGame::PrepareFrame() { _session.PrepareFrame(); }
DeviceInput WorkerGame::ReadInput() { return _session.ReadInput(); }
void WorkerGame::HandleInput(const DeviceInput& input) {
    Game::HandleInput(input);
    _session.Capture();
}
float WorkerGame::FrameSeconds(float seconds) { return _session.Advance(seconds); }
double WorkerGame::DrawSeconds() const { return _session.Scene().Seconds(); }
void WorkerGame::CanvasReady(const Texture2D& texture) const { _session.CanvasReady(texture); }
void WorkerGame::Stop() { _session.Stop(); }
