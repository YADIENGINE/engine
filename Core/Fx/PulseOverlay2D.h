#pragma once
#include <raylib.h>
#include "Utilities/Rect.h"

// Kalp atışı gibi iki vuruşla nabız atan renk örtüsü ve kenar karartması; etkinken yoğunluk 'fadeSpeed' ile yükselir.
struct PulseOverlayStyle {
    float fadeSpeed = 1.0f;
    float period = 1.0f;
    float secondBeatAt = 0.0f;
    float secondBeatScale = 0.0f;
    float decay = 1.0f;
    float tintAlpha = 0.0f;
    float tintBeatAlpha = 0.0f;
    int vignetteSize = 0;
    float vignetteAlpha = 0.0f;
    float vignetteBeatAlpha = 0.0f;
};

class PulseOverlay2D {
public:
    explicit PulseOverlay2D(const PulseOverlayStyle& style);
    void SetActive(bool active, Color color);
    void Update(float deltaSeconds);
    void Clear();
    void DrawTint(const Rect& area) const;
    void DrawVignette(const Rect& area) const;

private:
    [[nodiscard]] float Heartbeat() const;
    PulseOverlayStyle _style;
    bool _active = false;
    Color _color{};
    float _intensity = 0.0f;
    float _time = 0.0f;
};
