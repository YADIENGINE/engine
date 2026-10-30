#pragma once
#include <cmath>

// Animasyon eğrileri ve zaman yardımcıları. Eğrilerde t 0..1 aralığında verilir, 0'da 0 ve 1'de 1 döner.
// Header-only.
constexpr float Clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

// Yavaş başlar, hızlanarak biter.
constexpr float EaseInCubic(float t) {
    const float c = Clamp01(t);
    return c * c * c;
}

// Hızlı başlar, yumuşak durur.
constexpr float EaseOutCubic(float t) {
    const float u = 1.0f - Clamp01(t);
    return 1.0f - u * u * u;
}

// Yavaş başlar, ortada hızlanır, yavaş biter.
constexpr float EaseInOutCubic(float t) {
    const float c = Clamp01(t);
    if (c < 0.5f) {
        return 4.0f * c * c * c;
    }
    const float u = -2.0f * c + 2.0f;
    return 1.0f - u * u * u / 2.0f;
}

// Hedefi biraz aşıp geri gelir (pop-in).
constexpr float EaseOutBack(float t) {
    constexpr float overshoot = 1.70158f;
    const float u = Clamp01(t) - 1.0f;
    return 1.0f + (overshoot + 1.0f) * u * u * u + overshoot * u * u;
}

// Zaman çizelgesi yardımcıları: 't' (yaş) ile anlar arası.
// 't'nin 'from'..'to' aralığındaki ilerlemesi (0..1'e kırpılır).
constexpr float Progress01(float t, float from, float to) { return Clamp01((t - from) / (to - from)); }
// 'moment'tan beri geçen süre; öncesinde eksi.
constexpr float Since(float t, float moment) { return t - moment; }
// 'at' anında tam vurup 'seconds' içinde (EaseOutCubic) sönen darbe; öncesinde 0.
constexpr float EaseOutPulse(float t, float at, float seconds) {
    return t < at ? 0.0f : 1.0f - EaseOutCubic(Clamp01((t - at) / seconds));
}
// Sırayla beliren öğeler: 'index'. öğe 'delay + index × step' anında başlar, 'seconds' sürede 0'dan 1'e çıkar.
constexpr float Staggered(float t, float delay, float index, float step, float seconds) {
    return Clamp01((t - delay - index * step) / seconds);
}
// 'before'dan 'after'a ilerlerken 'moment' anı geçildi mi (tam o an dahil, bir kez).
constexpr bool Crossed(float before, float after, float moment) { return before < moment && after >= moment; }

// 'value'yu 'amount' kadar azaltır, 0'ın altına inmez. Sönen sayaçlar için: x = CountDown(x, dt / Seconds).
constexpr float CountDown(float value, float amount) { return value > amount ? value - amount : 0.0f; }

// Biten bir sürenin son 'threshold' saniyesinde 'period' aralıkla yanıp sönme; sönük yarıda true.
inline bool BlinkOff(float remaining, float threshold, float period) {
    return remaining <= threshold && std::fmod(remaining, period) < period / 2.0f;
}
