#pragma once
#include "Scene/LoadingScene.h"
#include "Utilities/Geometry/Vec2.h"

// The engine's own loading screen, for a game that gives none. It needs no font, file or sound, because a game may have
// none of them, and it takes no finish from the game. Something on it always moves while the sounds load.
class EngineLoadingScene final : public LoadingScene {
public:
    explicit EngineLoadingScene(Vec2 canvas);
    [[nodiscard]] bool Finished() const override;
    // How much of the picture shows: 0 black, 1 full.
    [[nodiscard]] float Visibility() const;

private:
    void Update(float seconds) override;
    void HandleInput(const DeviceInput& input) override;
    void Draw(Painter& paint) const override;
    void BeginFade(float seconds);
    [[nodiscard]] bool Fading() const { return _fadeSeconds > 0.0f; }

    Vec2 _canvas;
    float _seconds = 0.0f; // Since the screen opened; keeps growing while the sounds load
    float _fadeFrom = 1.0f;
    float _fadeSeconds = 0.0f;
    float _fadeElapsed = 0.0f;
};
