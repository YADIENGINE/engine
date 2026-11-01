#pragma once
#include "Editor/Worker/WorkerSession.h"
#include "Runtime/Game.h"

class WorkerGame : public Game {
public:
    WorkerGame(const GameSettings& settings, const SceneRegistry& registry, ControlChannel& channel,
               const std::string& session);
    ~WorkerGame() override;

protected:
    void Start() override;
    void PrepareFrame() override;
    DeviceInput ReadInput() override;
    void HandleInput(const DeviceInput& input) override;
    float FrameSeconds(float seconds) override;
    [[nodiscard]] double DrawSeconds() const override;
    void CanvasReady(const Texture2D& texture) const override;
    void Stop() override;

private:
    WorkerSession _session;
};
