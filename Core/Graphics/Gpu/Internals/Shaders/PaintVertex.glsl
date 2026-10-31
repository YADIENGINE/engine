#version 330 core
// Paint pipeline vertex stage. Attribute layout: PaintVertex (PaintVertex.h).
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aLocal;
layout(location = 2) in vec4 aShape;
layout(location = 3) in vec2 aStyle;
layout(location = 4) in vec4 aColor;
layout(location = 5) in vec4 aColor2;
layout(location = 6) in uvec4 aMeta; // kind, slot, emission, flags

uniform vec2 uViewSize; // Logical size of the pass (y grows down)

out vec2 vLocal;
out vec4 vShape;
out vec2 vStyle;
out vec4 vColor;
out vec4 vColor2;
flat out uvec4 vMeta;

void main() {
    vec2 ndc = vec2(aPosition.x / uViewSize.x * 2.0 - 1.0, 1.0 - aPosition.y / uViewSize.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vLocal = aLocal;
    vShape = aShape;
    vStyle = aStyle;
    vColor = aColor;
    vColor2 = aColor2;
    vMeta = aMeta;
}
