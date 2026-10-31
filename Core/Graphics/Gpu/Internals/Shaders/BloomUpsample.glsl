#version 330 core
// Dual-filter upsample (tent of eight taps), added onto the larger level (alpha 0 under premultiplied blending).
in vec2 vUv;
uniform sampler2D uTextures[8];
uniform vec2 uSourceTexel;
uniform float uBloomSpread;
layout(location = 0) out vec4 oColor;

void main() {
    vec2 offset = uSourceTexel * uBloomSpread;
    vec3 sum = texture(uTextures[0], vUv + vec2(-offset.x * 2.0, 0.0)).rgb;
    sum += texture(uTextures[0], vUv + vec2(offset.x * 2.0, 0.0)).rgb;
    sum += texture(uTextures[0], vUv + vec2(0.0, -offset.y * 2.0)).rgb;
    sum += texture(uTextures[0], vUv + vec2(0.0, offset.y * 2.0)).rgb;
    sum += texture(uTextures[0], vUv + vec2(-offset.x, -offset.y)).rgb * 2.0;
    sum += texture(uTextures[0], vUv + vec2(offset.x, -offset.y)).rgb * 2.0;
    sum += texture(uTextures[0], vUv + vec2(-offset.x, offset.y)).rgb * 2.0;
    sum += texture(uTextures[0], vUv + vec2(offset.x, offset.y)).rgb * 2.0;
    oColor = vec4(sum / 12.0, 0.0);
}
