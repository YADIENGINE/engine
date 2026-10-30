#pragma once
#include <raylib.h>
#include "Utilities/Rect.h"

// Ekran glitch'i: her 'fps'te yeniden dağılan renk şeritleri, koyu yırtıklar ve statik pikseller. Yükseklikler
// [min, max) aralığından seçilir; yoğunluk kalan süreyle söner.
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
    Color first = BLANK;
    Color second = BLANK;
    Color tear = BLANK;
    Color highlight = WHITE;
};

class GlitchEffects2D {
public:
    explicit GlitchEffects2D(const GlitchStyle& style);
    void Trigger(float seconds);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(const Rect& area, double timeSeconds) const;

private:
    GlitchStyle _style;
    float _life = 0.0f;
    float _duration = 0.0f;
};
