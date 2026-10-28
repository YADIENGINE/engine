#pragma once
#include "Editor/Preview/PreviewSession.h"
#include "Runtime/Game.h"

class PreviewGame : public Game {
public:
    PreviewGame(const GameSettings& settings, const PreviewRegistry& registry, ControlChannel& channel,
                const std::string& session);
    ~PreviewGame() override;

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
    PreviewSession _preview;
};
