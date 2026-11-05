#pragma once
#include <cstddef>
#include <vector>
#include "Fx/FxQuality.h"
#include "Fx/Screen/FlashLimit.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"

// A short light: an explosion's, or with no radius the whole scene's (lightning). It starts at full strength and falls
// away quickly.
struct LightFlashStyle {
    float radius = 0.0f;   // Where it reaches zero; 0: the whole scene, by raising the ambient, so nothing stops it
    float seconds = 0.3f;  // How long it lasts
    float strength = 1.0f; // At its start; multiplies the colour
};

[[nodiscard]] bool IsValid(const LightFlashStyle& style);

// The short lights playing now, as data for a LightFrame. They run on Update time, so a hitstop holds them with the
// rest of a hit. With flashes off there are none.
class FlashLights {
public:
    static constexpr std::size_t MaxPlaying = 8; // A new one replaces the oldest

    void SetQuality(const FxQuality& quality);
    // Plays if 'limit' allows it. An invalid style throws invalid_argument.
    void Start(FlashLimit& limit, Float2 at, Color4 color, const LightFlashStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    [[nodiscard]] LightFlashFrame Frame() const { return {.ambient = _ambient, .lights = _lights}; }

private:
    struct Playing {
        Float2 at;
        Color4 color;
        LightFlashStyle style;
        float elapsed = 0.0f;
    };
    void Describe();

    FxQuality _quality;
    std::vector<Playing> _playing;
    std::vector<LightSource> _lights;
    Color4 _ambient{0, 0, 0, 255};
};
