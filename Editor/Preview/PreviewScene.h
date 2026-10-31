#pragma once
#include <utility>
#include "Editor/Api/PreviewRegistry.h"
#include "Scene/Scene.h"

class PreviewScene : public Scene {
public:
    PreviewScene(Vec2 canvas, AudioSystem& audio, std::function<void(const std::string&)> log, const FontFace* font);
    ~PreviewScene() override;
    void Open(const PreviewDefinition& definition, const PreviewParameters& parameters);
    void Reset();
    [[nodiscard]] bool HasContent() const { return _preview != nullptr; }
    [[nodiscard]] const PreviewDefinition* Definition() const { return _definition; }
    void SetProfiler(Profiler* profiler) { _services.profiler = profiler; }
    [[nodiscard]] const PreviewParameters& Parameters() const { return _parameters; }
    [[nodiscard]] std::string SetParameter(const std::string& id, const PreviewValue& value);
    [[nodiscard]] std::string Action(const std::string& id);
    [[nodiscard]] bool CaptureParameters();
    [[nodiscard]] PreviewInspection Inspect() const;
    // Scaled seconds since the current preview instance was created; the host's draw clock.
    [[nodiscard]] double Seconds() const { return _seconds; }
    void SetApplicationCommand(std::function<std::string(const std::string&, const PreviewParameters&)> command) {
        _services.applicationCommand = std::move(command);
    }

protected:
    void InputCancelled() override;
    void HandleInput(const DeviceInput& input) override;
    void Update(float deltaSeconds) override;
    void Draw(Painter& paint) const override;
    void Stop() override;

private:
    // Objects (and their captured service references) die before services.
    double _seconds = 0.0;
    PreviewServices _services;
    // Reset commits old content's destruction before calling the new factory, even while playback is paused.
    ObjectList _content;
    const PreviewDefinition* _definition = nullptr;
    EditorPreview* _preview = nullptr;
    PreviewParameters _parameters;
};
