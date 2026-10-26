#pragma once

// Efektin yoğunluğu. Full: tam; Reduced: hafifletilmiş; Off: kapalı.
enum class EffectLevel { Full, Reduced, Off };

// Oyuncunun seçtiği görsel efekt yoğunluğu (eski bilgisayarlar ve ışığa duyarlılık için). Çizim yardımcıları (Neon,
// DrawGlow, scanline) ve Fx (parçacık, kamera sarsıntısı, flaş, glitch) bunu kendisi okur; çağıranlar dallanmaz.
// Game tercih dosyasından kurar.
struct EffectQuality {
    EffectLevel glow = EffectLevel::Full;  // Neon parıltıları ve DrawGlow
    int particlePercent = 100;             // Üretilen parçacık adedinin yüzdesi
    EffectLevel shake = EffectLevel::Full; // Kamera sarsıntısı, tekme ve zoom vuruşu
    bool sprintEffects = true;             // Sprintte kamera kayması ve hız çizgileri (Game: yılan izi)
    bool flashes = true;                   // Ekran flaşları ve glitch
    bool backgroundEffects = true;         // Scanline'lar ve kayan arka plan grid'i
};

[[nodiscard]] const EffectQuality& CurrentEffectQuality();
void SetEffectQuality(const EffectQuality& quality);
// Sarsıntı genliğinin çarpanı (Full 1, Reduced kısık, Off 0).
[[nodiscard]] float ShakeScale();
// 'count' parçacıktan particlePercent kadarı; sıfır olmayan istek en az bir parçacık üretir.
[[nodiscard]] int ScaledParticleCount(int count);
