#pragma once
#include <cstddef>
#include <vector>
#include "Fx/FxQuality.h"
#include "Fx/Screen/FlashLimit.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"
#include "Utilities/Random/Rng.h"

class Painter;

// A bolt of lightning, bright for a moment and flickering as it goes.
struct BoltStyle {
    float seconds = 0.26f; // How long it shows
    float width = 2.5f;    // Of its main line; branches are thinner
    float jag = 0.16f;     // How far it wanders from a straight line, against its length
    int branches = 2;
    float emission = 1.0f; // How strongly it feeds bloom
};

[[nodiscard]] bool IsValid(const BoltStyle& style);

// Bolts from above an area down to a point. Draw them after a LightLayer: they shine by themselves. They run on Update
// time, and with flashes off there are none.
class Bolts {
public:
    static constexpr std::size_t MaxPlaying = 4; // A new one replaces the oldest

    void SetQuality(const FxQuality& quality);
    // Strikes 'at' from above the top of 'area', if 'limit' allows it. An invalid style throws invalid_argument.
    void Strike(FlashLimit& limit, Float2 at, const FloatRect& area, Color4 color, const BoltStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(Painter& paint) const;
    [[nodiscard]] std::size_t ActiveCount() const { return _playing.size(); }

private:
    struct Line {
        std::vector<Float2> points;
        float width = 0.0f;
    };
    struct Playing {
        std::vector<Line> lines;
        Color4 color;
        BoltStyle style;
        float elapsed = 0.0f;
    };
    FxQuality _quality;
    std::vector<Playing> _playing;
    Rng _rng{1};
};
