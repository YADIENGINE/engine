#pragma once
#include <raylib.h>

enum class ParticleShape { Circle, Square, Pixel, Spark, Star };

struct ParticleDrawData {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float size;
    float rotation;
    float normalizedAge;
};
struct ParticleShapeStyle {
    float sparkTrailSeconds = 0.0f;
    float sparkThickness = 1.0f; // Çizime hazır piksel kalınlığı
};
// Parçacık simülasyonu ve statik küçük resimler aynı şekli çizer; tema/simülasyon bağımlılığı yoktur.
void DrawParticleShape(ParticleShape shape, const ParticleDrawData& data, ParticleShapeStyle style = {});
