#include "Editor/Preview/PreviewScene.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "Input/Input.hpp"

PreviewScene::PreviewScene(Vec2 canvas, const TextRenderer& text, AudioSystem& audio,
                           std::function<void(const std::string&)> log)
    : Scene(TimeDomain::Game)
    , _services{canvas, text, audio, std::move(log)} {}
PreviewScene::~PreviewScene() { ShutdownNoThrow(); }
void PreviewScene::Open(const PreviewDefinition& definition, const PreviewParameters& parameters) {
    _definition = &definition;
    std::vector<std::string> messages;
    _parameters = RestorePreviewParameters(definition, parameters, messages);
    for (const auto& message : messages)
        _services.log(message);
    Reset();
}
void PreviewScene::Reset() {
    if (!_definition) return;
    _preview = nullptr;
    _content.Clear();
    _content.Start(); // Commit removal: Stop and destruction happen before the new factory.
    _seconds = 0.0;
    auto instance = _definition->create(_services, _parameters);
    if (!instance) throw std::runtime_error("Preview factory returned an empty instance");
    _preview = instance.get();
    _content.Add(std::move(instance));
    _content.Start(); // Even paused content starts before it can receive input or draw.
    (void)CaptureParameters();
}
std::string PreviewScene::SetParameter(const std::string& id, const PreviewValue& value) {
    if (!_preview || !_definition) return "No active preview";
    const auto parameter = std::ranges::find(_definition->parameters, id, &PreviewParameter::id);
    if (parameter == _definition->parameters.end()) return "Unknown parameter: " + id;
    if (auto error = ValidatePreviewValue(*parameter, value); !error.empty()) return error;
    if (parameter->apply != PreviewApplyMode::OnReset) {
        if (auto error = _preview->ApplyParameter(id, value); !error.empty()) return error;
    }
    _parameters[id] = value;
    return {};
}
std::string PreviewScene::Action(const std::string& id) {
    if (!_preview || !_definition) return "No active preview";
    if (std::ranges::find(_definition->actions, id, &PreviewAction::id) == _definition->actions.end())
        return "Unknown preview action: " + id;
    return _preview->ExecuteAction(id);
}
bool PreviewScene::CaptureParameters() {
    return _preview && _definition && MergePreviewCapture(*_definition, _preview->CaptureParameters(), _parameters);
}
void PreviewScene::HandleInput(const DeviceInput& input) {
    if (_preview && _preview->Enabled()) _preview->HandleInput(input);
}
void PreviewScene::InputCancelled() {
    DeviceInput input;
    input.cancelled = true;
    if (_preview) _preview->HandleInput(input);
}
void PreviewScene::Update(float deltaSeconds) {
    _seconds += deltaSeconds;
    _content.Update(deltaSeconds);
}
void PreviewScene::Draw() const { _content.Draw(); }
PreviewInspection PreviewScene::Inspect() const { return _preview ? _preview->Inspect() : PreviewInspection{}; }
void PreviewScene::Stop() {
    _preview = nullptr;
    _content.Stop();
}
