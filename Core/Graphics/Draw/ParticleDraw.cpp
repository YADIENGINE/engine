#include "Graphics/Draw/ParticleDraw.h"
#include <cmath>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Affine2.h"
#include "Utilities/FloatRect.h"

void DrawParticleShape(Painter& paint, ParticleShape shape, const ParticleDrawData& data,
                       const ParticleShapeStyle& style) {
    switch (shape) {
    case ParticleShape::Circle: paint.Circle(data.position, data.size, Fill(data.color)); break;
    case ParticleShape::Square: {
        auto turned = paint.Transform(Affine2::Rotate(data.rotation, data.position));
        paint.Rect(FloatRect::FromCenter(data.position, {data.size * 2.0f, data.size * 2.0f}), Fill(data.color));
        break;
    }
    case ParticleShape::Pixel: {
        // Whole units, snapped: a pixel-art square that never blurs.
        const float half = std::round(data.size);
        paint.Rect({std::round(data.position.x) - half, std::round(data.position.y) - half, half * 2.0f, half * 2.0f},
                   Fill(data.color));
        break;
    }
    case ParticleShape::Spark:
        paint.Line(data.position - data.velocity * style.sparkTrailSeconds, data.position,
                   {.width = style.sparkThickness, .color = data.color});
        break;
    case ParticleShape::Star: DrawStar(paint, data.position, data.size, Fill(data.color)); break;
    }
}
