#pragma once
#include "Fx/FxQuality.h"
#include "Utilities/Color4.h"
#include "Utilities/FloatRect.h"

class Painter;

// Screen glitch: color strips, dark tears and static pixels redistributed at 'fps'. Heights are chosen from
// [min, max); intensity fades with remaining time.
struct GlitchStyle {
    float fps = 24.0f;
    int bands = 0;
    int minHeight = 1;
    int maxHeight = 1;
    int tearMinHeight = 1;
    int tearMaxHeight = 1;
    float shift = 0.0f;
    float bandAlpha = 0.0f;
    int tears = 0;
    float tearAlpha = 0.0f;
    int noise = 0;
    int noiseSize = 1;
    float noiseAlpha = 0.0f;
    Color4 first{0, 0, 0, 0};
    Color4 second{0, 0, 0, 0};
    Color4 tear{0, 0, 0, 0};
    Color4 highlight{255, 255, 255, 255};
};

class GlitchEffects2D {
public:
    explicit GlitchEffects2D(const GlitchStyle& style);
    // Flashes off: Trigger does nothing.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    void Trigger(float seconds);
    void Update(float deltaSeconds);
    void Clear();
    // Clipped to 'area'; 'timeSeconds' picks the pattern.
    void Draw(Painter& paint, const FloatRect& area, double timeSeconds) const;

private:
    GlitchStyle _style;
    FxQuality _quality;
    float _life = 0.0f;
    float _duration = 0.0f;
};
