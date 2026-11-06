#include "Editor/Worker/WorkerScene.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

WorkerScene::WorkerScene(SceneServices services)
    : Scene(TimeDomain::Game)
    , _services(std::move(services)) {}
namespace {
void RequireRunning(bool running) {
    if (!running) throw std::logic_error("The worker scene opens content only while it runs");
}
} // namespace

void WorkerScene::Open(const SceneDefinition& definition, const SceneParameters& parameters) {
    RequireRunning(_running);
    _definition = &definition;
    std::vector<std::string> messages;
    _parameters = RestoreSceneParameters(definition, parameters, messages);
    for (const auto& message : messages)
        _services.log(message);
    Reset();
}
void WorkerScene::Close() {
    const auto scene = std::move(_scene);
    if (scene) scene->Shutdown();
}
void WorkerScene::Reset() {
    if (!_definition) return;
    RequireRunning(_running);
    Close(); // Stop and destruction happen before the new factory, even while playback is paused.
    _seconds = 0.0;
    auto instance = _definition->create(_services, _parameters);
    if (!instance) throw std::runtime_error("Scene factory returned an empty instance");
    try {
        instance->Activate(); // Even paused content starts before it can receive input or draw.
    } catch (...) {
        try {
            instance->Shutdown(); // Whatever started is undone while the instance is alive
        } catch (...) {
            // The activation failure is the one to report.
        }
        throw;
    }
    _scene = std::move(instance);
    (void)MergeCapture();
}
std::string WorkerScene::SetParameter(const std::string& id, const SceneValue& value) {
    if (!_scene || !_definition) return "No active scene";
    const auto parameter = std::ranges::find(_definition->parameters, id, &SceneParameter::id);
    if (parameter == _definition->parameters.end()) return "Unknown parameter: " + id;
    if (auto error = ValidateSceneValue(*parameter, value); !error.empty()) return error;
    if (parameter->apply != SceneApplyMode::OnReset) {
        if (auto error = _scene->ApplyParameter(id, value); !error.empty()) return error;
    }
    _parameters[id] = value;
    return {};
}
std::string WorkerScene::Action(const std::string& id) {
    if (!_scene || !_definition) return "No active scene";
    if (std::ranges::find(_definition->actions, id, &SceneAction::id) == _definition->actions.end())
        return "Unknown scene action: " + id;
    return _scene->ExecuteAction(id);
}
bool WorkerScene::MergeCapture() {
    return _scene && _definition && MergeSceneCapture(*_definition, _scene->CaptureParameters(), _parameters);
}
SceneInspection WorkerScene::Inspect() const { return _scene ? _scene->Inspect() : SceneInspection{}; }
bool WorkerScene::GameTimePaused() const { return _scene && _scene->GameTimePaused(); }
// Game::Run's order for one scene: real time, then a Real scene's tick, then GameTimePaused, then a Game scene's tick.
void WorkerScene::UpdateReal(float realSeconds) {
    _seconds += realSeconds;
    if (!_scene) return;
    _scene->TickReal(realSeconds);
    if (_scene->Clock() == TimeDomain::Real) _scene->Tick(realSeconds);
}
void WorkerScene::Update(float gameSeconds) {
    if (_scene && _scene->Clock() == TimeDomain::Game) _scene->Tick(gameSeconds);
}
void WorkerScene::HandleInput(const DeviceInput& input) {
    if (_scene) _scene->ReadInput(input);
}
void WorkerScene::InputCancelled() {
    // At shutdown, Stop shuts the hosted scene down, which cancels its input itself.
    if (_scene && !IsShutdown()) _scene->CancelInput();
}
void WorkerScene::Draw(Painter& paint) const {
    if (_scene) _scene->Render(paint);
}
void WorkerScene::Start() { _running = true; }
void WorkerScene::Stop() {
    _running = false;
    Close();
}
