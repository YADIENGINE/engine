#include "Fx/GlitchEffects2D.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/MathUtils.h"

GlitchEffects2D::GlitchEffects2D(const GlitchStyle& style)
    : _style(style) {}

void GlitchEffects2D::Trigger(float seconds) {
    if (!std::isfinite(seconds) || !_quality.flashes) return;
    _duration = std::max(seconds, _life);
    _life = _duration;
}
void GlitchEffects2D::Update(float deltaSeconds) {
    if (deltaSeconds > 0.0f && std::isfinite(deltaSeconds)) _life = CountDown(_life, deltaSeconds);
}
void GlitchEffects2D::Clear() {
    _life = 0.0f;
    _duration = 0.0f;
}

void GlitchEffects2D::Draw(Painter& paint, const FloatRect& area, double timeSeconds) const {
    if (_life <= 0.0f || _duration <= 0.0f || area.Empty()) return;
    const float strength = _life / _duration;
    const auto frame = static_cast<unsigned>(timeSeconds * _style.fps);
    // Whole units in [0, range) for this frame and 'salt'.
    const auto random = [&](unsigned salt, float range) {
        const int span = std::max(static_cast<int>(range), 1);
        return static_cast<float>(Hash(frame * 7919U + salt) % static_cast<unsigned>(span));
    };
    const auto rows = [](int from, int to) { return static_cast<float>(to - from); };
    auto clip = paint.Clip(area);
    // Tears: dark horizontal strips, as if torn out of the play field.
    for (int i = 0; i < _style.tears; ++i) {
        const auto salt = 900U + static_cast<unsigned>(i) * 3U;
        if (random(salt, 100.0f) > strength * 100.0f) continue;
        const float height = static_cast<float>(_style.tearMinHeight) +
                             random(salt + 1U, rows(_style.tearMinHeight, _style.tearMaxHeight));
        const float y = area.y + random(salt + 2U, area.height - height);
        paint.Rect({area.x, y, area.width, height}, Fill(WithAlpha(_style.tear, _style.tearAlpha * strength)));
    }
    // Shifted colour bands: each splits into a red / cyan pair moving apart.
    {
        auto light = paint.Blending(Blend::Add);
        for (int i = 0; i < _style.bands; ++i) {
            const auto salt = static_cast<unsigned>(i) * 31U;
            const float height =
                static_cast<float>(_style.minHeight) + random(salt + 1U, rows(_style.minHeight, _style.maxHeight));
            const float y = area.y + random(salt + 2U, area.height - height);
            const float x = area.x + random(salt + 3U, area.width * 0.5f);
            const float width = std::floor(area.width * 0.25f) + random(salt + 4U, area.width * 0.5f);
            const float shift = std::trunc(_style.shift * strength) * (random(salt + 5U, 2.0f) == 0.0f ? 1.0f : -1.0f);
            paint.Rect({x + shift, y, width, height}, Fill(WithAlpha(_style.first, _style.bandAlpha * strength)));
            paint.Rect({x - shift, y + std::floor(height / 3.0f), width, height},
                       Fill(WithAlpha(_style.second, _style.bandAlpha * strength)));
        }
    }
    // Static: random bright large pixels.
    const int noise = static_cast<int>(static_cast<float>(_style.noise) * strength);
    const auto size = static_cast<float>(_style.noiseSize);
    for (int i = 0; i < noise; ++i) {
        const auto salt = 5000U + static_cast<unsigned>(i) * 3U;
        const float kind = random(salt + 2U, 3.0f);
        const Color4 color = kind == 0.0f ? _style.first : kind == 1.0f ? _style.second : _style.highlight;
        paint.Rect({area.x + random(salt, area.width), area.y + random(salt + 1U, area.height), size, size},
                   Fill(WithAlpha(color, _style.noiseAlpha * strength)));
    }
}
