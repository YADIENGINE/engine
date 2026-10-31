#version 330 core
// Final image: canvas colour (slot 0) with chromatic aberration, bloom (slot 1), scanlines and vignette.
in vec2 vUv;
uniform sampler2D uTextures[8];
uniform vec2 uSourceTexel;
uniform float uBloomIntensity;
uniform float uVignette;
uniform float uScanlines;
uniform float uScanlineCount;
uniform float uAberration;
layout(location = 0) out vec4 oColor;

void main() {
    vec3 color;
    if (uAberration > 0.0) {
        vec2 shift = (vUv - 0.5) * 2.0 * uAberration * uSourceTexel;
        color.r = texture(uTextures[0], vUv + shift).r;
        color.g = texture(uTextures[0], vUv).g;
        color.b = texture(uTextures[0], vUv - shift).b;
    } else {
        color = texture(uTextures[0], vUv).rgb;
    }
    if (uBloomIntensity > 0.0) color += texture(uTextures[1], vUv).rgb * uBloomIntensity;
    if (uScanlines > 0.0) {
        float line = 0.5 + 0.5 * cos(vUv.y * uScanlineCount * 6.28318530);
        color *= 1.0 - uScanlines * line;
    }
    if (uVignette > 0.0) {
        float edge = smoothstep(0.35, 0.85, length(vUv - 0.5) * 1.41421356);
        color *= 1.0 - uVignette * edge;
    }
    oColor = vec4(color, 1.0);
}
