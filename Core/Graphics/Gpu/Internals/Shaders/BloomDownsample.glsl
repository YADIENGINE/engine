#version 330 core
// Dual-filter downsample (centre + four diagonal taps at half-texel offsets).
in vec2 vUv;
uniform sampler2D uTextures[8];
uniform vec2 uSourceTexel;
uniform float uBloomSpread;
layout(location = 0) out vec4 oColor;

void main() {
    vec2 offset = uSourceTexel * uBloomSpread;
    vec3 sum = texture(uTextures[0], vUv).rgb * 4.0;
    sum += texture(uTextures[0], vUv + vec2(-offset.x, -offset.y)).rgb;
    sum += texture(uTextures[0], vUv + vec2(offset.x, -offset.y)).rgb;
    sum += texture(uTextures[0], vUv + vec2(-offset.x, offset.y)).rgb;
    sum += texture(uTextures[0], vUv + vec2(offset.x, offset.y)).rgb;
    oColor = vec4(sum / 8.0, 1.0);
}
