#pragma once
#include <raylib.h>
#include "Graphics/Draw/ParticleDraw.hpp"

// Eski yol (raylib); ParticleShape ve Painter karşılığı Graphics/Draw/ParticleDraw.hpp'de. Bu başlık göç bitince
// silinir.
struct LegacyParticleDrawData {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float size;
    float rotation;
    float normalizedAge;
};
void DrawParticleShape(ParticleShape shape, const LegacyParticleDrawData& data, ParticleShapeStyle style = {});
