#include "Graphics/Draw/ParticleDraw.h"
#include <cmath>
#include "Graphics/Draw/DrawUtils.h"

void DrawParticleShape(ParticleShape shape, const ParticleDrawData& data, ParticleShapeStyle style) {
    switch (shape) {
    case ParticleShape::Circle: DrawCircleV(data.position, data.size, data.color); break;
    case ParticleShape::Square:
        DrawRectanglePro({data.position.x, data.position.y, data.size * 2.0f, data.size * 2.0f}, {data.size, data.size},
                         data.rotation, data.color);
        break;
    case ParticleShape::Pixel: {
        const float half = std::round(data.size);
        DrawRectangleRec(
            {std::round(data.position.x) - half, std::round(data.position.y) - half, half * 2.0f, half * 2.0f},
            data.color);
        break;
    }
    case ParticleShape::Spark: {
        const Vector2 tail{data.position.x - data.velocity.x * style.sparkTrailSeconds,
                           data.position.y - data.velocity.y * style.sparkTrailSeconds};
        DrawLineEx(tail, data.position, style.sparkThickness, data.color);
        break;
    }
    case ParticleShape::Star: DrawStar(data.position, data.size, data.color); break;
    }
}
