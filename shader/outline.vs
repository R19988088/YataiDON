#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 2) in vec3 vertexNormal;
layout(location = 3) in vec4 vertexColor;

uniform mat4 mvp;
uniform float outlineThickness;
uniform vec2 screenSize;

void main() {
    vec4 clipPos = mvp * vec4(vertexPosition, 1.0);
    vec4 clipNormal = mvp * vec4(vertexNormal, 0.0);

    vec2 n = clipNormal.xy;
    float len = length(n);
    if (len > 0.0001)
        n = n / len;

    clipPos.xy += n * (outlineThickness / screenSize) * vertexColor.a * 20.0;
    clipPos.z += 0.001;

    gl_Position = clipPos;
}
