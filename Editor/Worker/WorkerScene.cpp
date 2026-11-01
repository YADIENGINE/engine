#include "Editor/Worker/WorkerScene.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "Input/Input.h"

WorkerScene::WorkerScene(Vec2 canvas, AudioSystem& audio, std::function<void(const std::string&)> log,
                         const FontFace* font)
    : Scene(TimeDomain::Game)
    , _services{canvas, audio, std::move(log), font} {}
WorkerScene::~WorkerScene() { ShutdownNoThrow(); }
void WorkerScene::Open(const SceneDefinition& definition, const SceneParameters& parameters) {
    _definition = &definition;
    std::vector<std::string> messages;
    _parameters = RestoreSceneParameters(definition, parameters, messages);
    for (const auto& message : messages)
        _services.log(message);
    Reset();
}
void WorkerScene::Reset() {
    if (!_definition) return;
    _scene = nullptr;
    _content.Clear();
    _content.Start(); // Commit removal: Stop and destruction happen before the new factory.
    _seconds = 0.0;
    auto instance = _definition->create(_services, _parameters);
    if (!instance) throw std::runtime_error("Scene factory returned an empty instance");
    _scene = instance.get();
    _content.Add(std::move(instance));
    _content.Start(); // Even paused content starts before it can receive input or draw.
    (void)CaptureParameters();
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
bool WorkerScene::CaptureParameters() {
    return _scene && _definition && MergeSceneCapture(*_definition, _scene->CaptureParameters(), _parameters);
}
void WorkerScene::HandleInput(const DeviceInput& input) {
    if (_scene && _scene->Enabled()) _scene->HandleInput(input);
}
void WorkerScene::InputCancelled() {
    DeviceInput input;
    input.cancelled = true;
    if (_scene) _scene->HandleInput(input);
}
void WorkerScene::Update(float deltaSeconds) {
    _seconds += deltaSeconds;
    _content.Update(deltaSeconds);
}
void WorkerScene::Draw(Painter& paint) const { _content.Draw(paint); }
SceneInspection WorkerScene::Inspect() const { return _scene ? _scene->Inspect() : SceneInspection{}; }
void WorkerScene::Stop() {
    _scene = nullptr;
    _content.Stop();
}
