#pragma once
#include <raylib.h>
#include <array>
#include "Fx/ParticleSystem2D.h"

// Hazır parçacık patlamaları. Soft: yumuşak daire + kıvılcım; Spark: kıvılcım; Sparkle: yükselen yıldız; Debris: düşen
// kare + kıvılcım; Pixel: düşen piksel; Confetti: süzülen kare.
enum class ParticleRecipe { Soft, Spark, Sparkle, Debris, Pixel, Confetti };

// Tariflerin ortak ölçüleri; görünüşü tema verir (Theme::fx.particles). Hız çarpanları emission.speed'i ölçekler,
// yerçekimleri aşağı doğru ivmedir (negatif: yükselir), oranlar ikinci katmanın ana katmana göre adedidir.
struct ParticleRecipeTuning {
    ParticleEmission emission; // Patlama katmanlarının ortak yayılımı
    float endSizeScale = 1.0f;
    float sparkTrailSeconds = 0.0f;
    float sparkThickness = 1.0f;
    float sparkMinThickness = 0.0f;
    float starScale = 1.0f;
    float pixelScale = 1.0f;
    float sparkSpeed = 1.0f;
    float softSparkRatio = 0.0f;
    float sparkleSpeed = 1.0f;
    float sparkleGravity = 0.0f;
    float sparkleSoftRatio = 0.0f;
    float debrisSpeed = 1.0f;
    float debrisGravity = 0.0f;
    float debrisSparkRatio = 0.0f;
    float pixelGravity = 0.0f;
    float confettiSpeed = 1.0f;
    float confettiGravity = 0.0f;
    int trailCount = 0; // İz: hareket eden nesnenin arkasında kalan ışık
    float trailSpeed = 1.0f;
    int dashCount = 0; // Atılma: bakılan yönün tersine kıvılcım
    float dashSpeed = 1.0f;
    float dashSpread = 0.0f;
    int steamCount = 0; // Buhar: yukarı süzülen, büyüyerek sönen pus
    ParticleEmission steam;
    float steamEndSizeScale = 1.0f;
    float steamWhiten = 0.0f;
    float steamOpacity = 1.0f;
};

// Tek bir yayım katmanı; 'share' patlamada katmanın istenen adetten aldığı pay.
struct ParticleLayer {
    float share = 0.0f;
    ParticleEmission emission;
    ParticleAppearance appearance;
};
using ParticleBurst = std::array<ParticleLayer, 2>;

[[nodiscard]] ParticleBurst BurstRecipe(ParticleRecipe recipe, const ParticleRecipeTuning& tuning);
[[nodiscard]] ParticleLayer TrailRecipe(const ParticleRecipeTuning& tuning);
// 'direction': hareket yönü; kıvılcımlar tersine saçılır.
[[nodiscard]] ParticleLayer DashRecipe(Vector2 direction, const ParticleRecipeTuning& tuning);
[[nodiscard]] ParticleLayer SteamRecipe(const ParticleRecipeTuning& tuning);

// Katmanı 'color' renginde 'count' adet yayar.
void EmitLayer(ParticleSystem2D& particles, Vector2 position, Color color, int count, const ParticleLayer& layer);
// 'count' adedi tarifin katmanlarına paylarıyla dağıtır.
void EmitBurst(ParticleSystem2D& particles, Vector2 position, Color color, int count, ParticleRecipe recipe,
               const ParticleRecipeTuning& tuning);
