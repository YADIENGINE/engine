#pragma once
#include <utility>
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"

class WorkerScene : public Scene {
public:
    WorkerScene(Vec2 canvas, AudioSystem& audio, std::function<void(const std::string&)> log, const FontFace* font);
    ~WorkerScene() override;
    void Open(const SceneDefinition& definition, const SceneParameters& parameters);
    void Reset();
    [[nodiscard]] bool HasContent() const { return _scene != nullptr; }
    [[nodiscard]] const SceneDefinition* Definition() const { return _definition; }
    void SetProfiler(Profiler* profiler) { _services.profiler = profiler; }
    [[nodiscard]] const SceneParameters& Parameters() const { return _parameters; }
    [[nodiscard]] std::string SetParameter(const std::string& id, const SceneValue& value);
    [[nodiscard]] std::string Action(const std::string& id);
    [[nodiscard]] bool CaptureParameters();
    [[nodiscard]] SceneInspection Inspect() const;
    // Scaled seconds since the current scene instance was created; the host's draw clock.
    [[nodiscard]] double Seconds() const { return _seconds; }
    void SetApplicationCommand(std::function<std::string(const std::string&, const SceneParameters&)> command) {
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
    SceneServices _services;
    // Reset commits old content's destruction before calling the new factory, even while playback is paused.
    ObjectList _content;
    const SceneDefinition* _definition = nullptr;
    EditorScene* _scene = nullptr;
    SceneParameters _parameters;
};
