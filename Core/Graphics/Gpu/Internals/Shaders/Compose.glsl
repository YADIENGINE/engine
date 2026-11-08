#version 330 core
// Final image: canvas colour (slot 0) with wobble, tape shifts and chromatic aberration, bloom (slot 1), colour fade and
// tint, the tape's band, scanlines, vignette and grain. Every effect is off at 0 and skips its work.
in vec2 vUv;
uniform sampler2D uTextures[8];
uniform vec2 uSourceTexel;
uniform vec4 uRegion; // UV origin and size of the physical screen; outside is passed through unchanged.
uniform float uBloomIntensity;
uniform float uVignette;
uniform float uScanlines;
uniform float uScanlineCount;
uniform float uAberration;
uniform float uFade;
uniform vec3 uTint;
uniform float uGrain;
uniform float uGrainCell;  // One grain, in texels
uniform float uNoiseFrame; // Picks this moment's noise
uniform float uWobble;     // Largest move, in texels
uniform float uWobblePhase;
uniform float uTapeBand;
uniform float uTapePush;  // How far the band pushes the image, in texels
uniform float uTapeShift; // Largest sideways jump of a line, in texels
uniform float uTapeRoll;  // Where the band is, 0..1
uniform float uTapeLine;  // One tape line, in texels
layout(location = 0) out vec4 oColor;

// An integer hash (pcg3d, Jarzynski and Olano): no pattern shows, at any position or moment.
vec3 Noise(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return vec3(v) / 4294967295.0;
}

float Luma(vec3 color) { return dot(color, vec3(0.299, 0.587, 0.114)); }

vec2 WobbleOffset(vec2 uv) {
    float inset = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float p = uWobblePhase;
    vec2 wave = vec2(0.6 * sin(uv.y * 7.0 + p) + 0.4 * sin(uv.y * 15.0 + uv.x * 3.0 - p * 1.6875),
                     0.6 * sin(uv.x * 6.0 + p * 0.8125) + 0.4 * sin(uv.x * 13.0 + uv.y * 4.0 + p * 1.3125));
    vec2 fromCenter = uv - 0.5;
    wave += vec2(-fromCenter.y, fromCenter.x) * sin(p * 0.3125);
    return wave * smoothstep(0.0, 0.22, inset) * uWobble * uSourceTexel;
}

void main() {
    if (any(lessThan(vUv, uRegion.xy)) || any(greaterThanEqual(vUv, uRegion.xy + uRegion.zw))) {
        oColor = texture(uTextures[0], vUv);
        return;
    }
    vec2 screen = (vUv - uRegion.xy) / uRegion.zw;
    vec2 lo = uRegion.xy + uSourceTexel * 0.5;
    vec2 hi = uRegion.xy + uRegion.zw - uSourceTexel * 0.5;
    vec2 uv = vUv;
    if (uWobble > 0.0) uv += WobbleOffset(screen);

    float band = 0.0;
    float tapeLine = 0.0;
    if (uTapeBand > 0.0 || uTapeShift > 0.0) {
        tapeLine = floor((vUv.y - uRegion.y) / (uSourceTexel.y * uTapeLine));
        vec3 random = Noise(uvec3(uint(tapeLine), uint(uNoiseFrame), 7u));
        // Most lines stay nearly still; a few jump the whole way.
        uv.x += (random.x * 2.0 - 1.0) * random.z * random.z * uTapeShift * uSourceTexel.x;
        if (uTapeBand > 0.0) {
            float away = abs(screen.y - uTapeRoll);
            band = 1.0 - smoothstep(0.0, 0.055, min(away, 1.0 - away));
            uv.x += band * (0.6 + 0.8 * random.y) * uTapePush * uSourceTexel.x;
        }
    }

    vec3 color;
    if (uAberration > 0.0) {
        vec2 shift = (screen - 0.5) * 2.0 * uAberration * uSourceTexel;
        color.r = texture(uTextures[0], clamp(uv + shift, lo, hi)).r;
        color.g = texture(uTextures[0], clamp(uv, lo, hi)).g;
        color.b = texture(uTextures[0], clamp(uv - shift, lo, hi)).b;
    } else {
        color = texture(uTextures[0], clamp(uv, lo, hi)).rgb;
    }
    if (uBloomIntensity > 0.0) color += texture(uTextures[1], clamp(uv, lo, hi)).rgb * uBloomIntensity;
    if (uFade > 0.0) color = mix(color, vec3(Luma(color)), uFade);
    color *= uTint;
    if (band > 0.0) {
        uvec3 dash = uvec3(uint(gl_FragCoord.x / (uTapeLine * 12.0)), uint(tapeLine), uint(uNoiseFrame));
        color = mix(color, vec3(Luma(color)), band * uTapeBand * 0.6);
        color += band * band * uTapeBand * 0.3 * step(0.84, Noise(dash).x);
    }
    if (uScanlines > 0.0) {
        float line = 0.5 + 0.5 * cos(screen.y * uScanlineCount * 6.28318530);
        color *= 1.0 - uScanlines * line;
    }
    if (uVignette > 0.0) {
        float edge = smoothstep(0.35, 0.85, length(screen - 0.5) * 1.41421356);
        color *= 1.0 - uVignette * edge;
    }
    if (uGrain > 0.0) {
        vec3 random = Noise(uvec3(uvec2(gl_FragCoord.xy / uGrainCell), uint(uNoiseFrame)));
        float weight = mix(1.0, 0.3, clamp(Luma(color), 0.0, 1.0));
        color += (random.x + random.y - 1.0) * uGrain * 0.3 * weight;
    }
    oColor = vec4(color, 1.0);
}
