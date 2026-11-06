#pragma once
#include <memory>
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"

// Hosts the catalog's scene through Scene's public lifecycle. The worker's frame time is already the editor's (pause,
// Step, speed), so both clocks of the hosted scene follow it; the hosted scene's TimeDomain and GameTimePaused hold.
// Content lives between this scene's Start and Stop, so its Stop always shuts the hosted scene down.
class WorkerScene : public Scene {
public:
    explicit WorkerScene(SceneServices services);
    // Throws std::logic_error before this scene has started or after it stopped.
    void Open(const SceneDefinition& definition, const SceneParameters& parameters);
    // Stops and destroys the current instance before the factory makes the next one.
    void Reset();
    [[nodiscard]] bool HasContent() const { return _scene != nullptr; }
    [[nodiscard]] const SceneDefinition* Definition() const { return _definition; }
    [[nodiscard]] const SceneServices& Services() const { return _services; }
    [[nodiscard]] const SceneParameters& Parameters() const { return _parameters; }
    [[nodiscard]] std::string SetParameter(const std::string& id, const SceneValue& value);
    [[nodiscard]] std::string Action(const std::string& id);
    // Merges what the hosted scene reports changed into the accepted values; true when they changed.
    [[nodiscard]] bool MergeCapture();
    [[nodiscard]] SceneInspection Inspect() const override;
    [[nodiscard]] bool GameTimePaused() const override;
    // Playback seconds since the current scene instance was created; the host's draw clock.
    [[nodiscard]] double Seconds() const { return _seconds; }

protected:
    void Start() override;
    void UpdateReal(float realSeconds) override;
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    void Update(float gameSeconds) override;
    void Draw(Painter& paint) const override;
    void Stop() override;

private:
    void Close();

    double _seconds = 0.0;
    bool _running = false;   // Between Start and Stop
    SceneServices _services; // Outlives the hosted scene, which may reference it
    const SceneDefinition* _definition = nullptr;
    std::unique_ptr<Scene> _scene;
    SceneParameters _parameters;
};
