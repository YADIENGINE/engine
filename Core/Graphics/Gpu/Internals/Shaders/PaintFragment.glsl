#version 330 core
// Paint pipeline fragment stage. Kinds, modes and flags: PaintVertex.h. Colours are premultiplied.
in vec2 vLocal;
in vec4 vShape;
in vec2 vStyle;
in vec4 vColor;
in vec4 vColor2;
flat in uvec4 vMeta;

uniform sampler2D uTextures[8];

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oEmission;

const uint KindSolid = 0u;
const uint KindTexture = 1u;
const uint KindGlyph = 2u;
const uint KindGlyphHalo = 3u;
const uint KindCircle = 4u;
const uint KindEllipse = 5u;
const uint KindRoundRect = 6u;
const uint KindChamferRect = 7u;
const uint KindSegment = 8u;
const uint KindArc = 9u;
const uint KindGlow = 10u;
const uint KindComposite = 11u;

const uint ModeFill = 0u;
const uint ModeStroke = 1u;
const uint ModeHalo = 2u;
const uint FlagGradientLinear = 4u;
const uint FlagGradientRadial = 8u;
const uint FlagCapRound = 16u;
const uint FlagCapSquare = 32u;
const uint FlagCoverage = 64u;

// GLSL 3.30 only indexes sampler arrays with constants. textureLod: no derivatives needed in branches.
vec4 SampleSlot(uint slot, vec2 uv) {
    switch (int(slot)) {
    case 0: return textureLod(uTextures[0], uv, 0.0);
    case 1: return textureLod(uTextures[1], uv, 0.0);
    case 2: return textureLod(uTextures[2], uv, 0.0);
    case 3: return textureLod(uTextures[3], uv, 0.0);
    case 4: return textureLod(uTextures[4], uv, 0.0);
    case 5: return textureLod(uTextures[5], uv, 0.0);
    case 6: return textureLod(uTextures[6], uv, 0.0);
    case 7: return textureLod(uTextures[7], uv, 0.0);
    }
    return vec4(0.0);
}

// Signed distances: negative inside, in the shape's local units.
float RoundRectDistance(vec2 p, vec2 extent, float radius) {
    vec2 q = abs(p) - extent + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float ChamferRectDistance(vec2 p, vec2 extent, float chamfer) {
    float box = RoundRectDistance(p, extent, 0.0);
    vec2 a = abs(p);
    float cut = (a.x + a.y - (extent.x + extent.y - chamfer)) * 0.70710678;
    return max(box, cut);
}

float EllipseDistance(vec2 p, vec2 radii) {
    float k1 = length(p / radii);
    float k2 = length(p / (radii * radii));
    return k2 > 0.0 ? k1 * (k1 - 1.0) / k2 : -min(radii.x, radii.y);
}

float SegmentDistance(vec2 p, float halfLength, float halfWidth, uint flags) {
    if ((flags & FlagCapRound) != 0u) return length(vec2(max(abs(p.x) - halfLength, 0.0), p.y)) - halfWidth;
    float reach = (flags & FlagCapSquare) != 0u ? halfLength + halfWidth : halfLength;
    return RoundRectDistance(p, vec2(reach, halfWidth), 0.0);
}

// Arc centred on the +x axis, 'halfSweep' to each side; radius 'ra', half thickness 'rb'.
float ArcDistance(vec2 p, float ra, float rb, float halfSweep, uint flags) {
    vec2 sc = vec2(sin(halfSweep), cos(halfSweep));
    vec2 q = vec2(abs(p.y), p.x);
    bool outside = sc.y * q.x > sc.x * q.y;
    if ((flags & FlagCapRound) != 0u) return (outside ? length(q - sc * ra) : abs(length(q) - ra)) - rb;
    if (outside) {
        vec2 a = sc * (ra - rb);
        vec2 ab = sc * (2.0 * rb);
        float h = clamp(dot(q - a, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
        return length(q - a - ab * h);
    }
    return max(abs(length(q) - ra) - rb, dot(q, vec2(sc.y, -sc.x)));
}

void main() {
    uint kind = vMeta.x;
    uint slot = vMeta.y;
    float emission = float(vMeta.z) / 255.0;
    uint flags = vMeta.w;
    uint mode = flags & 3u;

    vec4 texel = vec4(1.0);
    float distance = 0.0;
    bool shaped = false;
    if (kind == KindTexture || kind == KindComposite) {
        texel = SampleSlot(slot, vLocal);
    } else if (kind == KindGlyph) {
        texel = vec4(SampleSlot(slot, vLocal).r);
    } else if (kind == KindGlyphHalo) {
        distance = (128.0 - SampleSlot(slot, vLocal).r * 255.0) / vShape.x; // Distance pixels, positive outside
        shaped = true;
    } else if (kind == KindCircle) {
        distance = length(vLocal) - vShape.x;
        shaped = true;
    } else if (kind == KindEllipse) {
        distance = EllipseDistance(vLocal, vShape.xy);
        shaped = true;
    } else if (kind == KindRoundRect) {
        distance = RoundRectDistance(vLocal, vShape.xy, vShape.z);
        shaped = true;
    } else if (kind == KindChamferRect) {
        distance = ChamferRectDistance(vLocal, vShape.xy, vShape.z);
        shaped = true;
    } else if (kind == KindSegment) {
        distance = SegmentDistance(vLocal, vShape.x, vShape.y, flags);
        shaped = true;
    } else if (kind == KindArc) {
        distance = ArcDistance(vLocal, vShape.x, vShape.y, vShape.z, flags);
        shaped = true;
    } else if (kind == KindGlow) {
        distance = length(vLocal) / vShape.x;
    }
    // Screen-space width of one pixel in distance units (uniform control flow).
    float pixel = max(fwidth(distance), 1e-5);

    float coverage = 1.0;
    if (kind == KindGlow) {
        float t = clamp(distance, 0.0, 1.0);
        coverage = 1.0 - t * t * (3.0 - 2.0 * t);
    } else if (shaped) {
        if (mode == ModeFill) {
            coverage = clamp(0.5 - distance / pixel, 0.0, 1.0);
        } else if (mode == ModeStroke) {
            float bandHalf = vStyle.x;
            float band = abs(distance - vStyle.y) - bandHalf;
            coverage = clamp(0.5 - band / pixel, 0.0, 1.0) * clamp(2.0 * bandHalf / pixel, 0.0, 1.0);
        } else {
            float t = clamp(max(distance, 0.0) / max(vStyle.x, 1e-5), 0.0, 1.0);
            coverage = (1.0 - t) * (1.0 - t);
        }
    }

    vec4 color = vColor;
    if ((flags & FlagGradientLinear) != 0u) {
        color = mix(vColor, vColor2, clamp(vStyle.x, 0.0, 1.0));
    } else if ((flags & FlagGradientRadial) != 0u) {
        color = mix(vColor, vColor2, clamp(length(vLocal) * vStyle.x, 0.0, 1.0));
    }
    if ((flags & FlagCoverage) != 0u) coverage *= vShape.w;
    vec4 result = color * texel * coverage;
    oColor = result;

    vec3 glow = result.rgb * emission;
    if (kind == KindComposite) glow += SampleSlot(uint(vShape.x + 0.5), vLocal).rgb * vColor.a;
    // Alpha follows coverage: emissive light under opaque drawing is covered like its colour.
    oEmission = vec4(glow, result.a);
}
