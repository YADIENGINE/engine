#pragma once
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"

class Painter;

enum class ParticleShape { Circle, Square, Pixel, Spark, Star };

struct ParticleDrawData {
    Float2 position;
    Float2 velocity;
    Color4 color;
    float size = 0.0f;
    float rotation = 0.0f; // Radians, clockwise
    float normalizedAge = 0.0f;
};
struct ParticleShapeStyle {
    float sparkTrailSeconds = 0.0f;
    float sparkThickness = 1.0f; // Line width, drawing units
};
// The particle simulation and static thumbnails draw the same shapes; no theme or simulation dependency.
void DrawParticleShape(Painter& paint, ParticleShape shape, const ParticleDrawData& data,
                       const ParticleShapeStyle& style = {});
