#pragma once
#include "Utilities/Color4.h"
#include "Utilities/FloatRect.h"

class Painter;

// Color overlay and edge darkening pulsing twice like a heartbeat; intensity rises at 'fadeSpeed' while active.
struct PulseOverlayStyle {
    float fadeSpeed = 1.0f;
    float period = 1.0f;
    float secondBeatAt = 0.0f;
    float secondBeatScale = 0.0f;
    float decay = 1.0f;
    float tintAlpha = 0.0f;
    float tintBeatAlpha = 0.0f;
    float vignetteSize = 0.0f;
    float vignetteAlpha = 0.0f;
    float vignetteBeatAlpha = 0.0f;
};

class PulseOverlay2D {
public:
    explicit PulseOverlay2D(const PulseOverlayStyle& style);
    void SetActive(bool active, Color4 color);
    void Update(float deltaSeconds);
    void Clear();
    void DrawTint(Painter& paint, const FloatRect& area) const;
    void DrawVignette(Painter& paint, const FloatRect& area) const;

private:
    [[nodiscard]] float Heartbeat() const;
    PulseOverlayStyle _style;
    bool _active = false;
    Color4 _color{};
    float _intensity = 0.0f;
    float _time = 0.0f;
};
