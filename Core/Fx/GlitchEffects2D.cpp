#include "Fx/GlitchEffects2D.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/EffectQuality.h"
#include "Utilities/MathUtils.h"

GlitchEffects2D::GlitchEffects2D(const GlitchStyle& style)
    : _style(style) {}

void GlitchEffects2D::Trigger(float seconds) {
    if (!std::isfinite(seconds) || !CurrentEffectQuality().flashes) return;
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

void GlitchEffects2D::Draw(const Rect& area, double timeSeconds) const {
    if (_life <= 0.0f || _duration <= 0.0f) return;
    const float strength = _life / _duration;
    const auto frame = static_cast<unsigned>(timeSeconds * _style.fps);
    const auto random = [&](unsigned salt, int range) {
        return static_cast<int>(Hash(frame * 7919U + salt) % static_cast<unsigned>(std::max(range, 1)));
    };
    BeginClip(area.x, area.y, area.width, area.height);
    // Yırtıklar: oyun alanından koparılmış gibi koyu yatay şeritler.
    for (int i = 0; i < _style.tears; ++i) {
        const auto salt = 900U + static_cast<unsigned>(i) * 3U;
        if (random(salt, 100) > static_cast<int>(strength * 100.0f)) continue;
        const int height = _style.tearMinHeight + random(salt + 1U, _style.tearMaxHeight - _style.tearMinHeight);
        const int y = area.y + random(salt + 2U, area.height - height);
        FillRect({area.x, y, area.width, height}, Fade(_style.tear, _style.tearAlpha * strength));
    }
    // Kayan renk şeritleri: her biri kırmızı / camgöbeği çifti olarak iki yana ayrışır.
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < _style.bands; ++i) {
        const auto salt = static_cast<unsigned>(i) * 31U;
        const int height = _style.minHeight + random(salt + 1U, _style.maxHeight - _style.minHeight);
        const int y = area.y + random(salt + 2U, area.height - height);
        const int x = area.x + random(salt + 3U, area.width / 2);
        const int width = area.width / 4 + random(salt + 4U, area.width / 2);
        const int shift = static_cast<int>(_style.shift * strength) * (random(salt + 5U, 2) == 0 ? 1 : -1);
        FillRect({x + shift, y, width, height}, Fade(_style.first, _style.bandAlpha * strength));
        FillRect({x - shift, y + height / 3, width, height}, Fade(_style.second, _style.bandAlpha * strength));
    }
    EndBlendMode();
    // Statik: rastgele parlayan iri pikseller.
    const int noise = static_cast<int>(static_cast<float>(_style.noise) * strength);
    for (int i = 0; i < noise; ++i) {
        const auto salt = 5000U + static_cast<unsigned>(i) * 3U;
        const int kind = random(salt + 2U, 3);
        const Color color = kind == 0 ? _style.first : kind == 1 ? _style.second : _style.highlight;
        FillRect({area.x + random(salt, area.width), area.y + random(salt + 1U, area.height), _style.noiseSize,
                  _style.noiseSize},
                 Fade(color, _style.noiseAlpha * strength));
    }
    EndScissorMode();
}
