#version 300 es
precision mediump float;

in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texSize;
uniform float outlineThickness;

void main() {
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / texSize;

    vec4 center = texture(texture0, uv);
    if (center.a > 0.001) {
        fragColor = vec4(center.rgb, 1.0);
        return;
    }

    vec2 kern = texel * outlineThickness;
    float accum = 0.0;
    accum += texture(texture0, uv + vec2(-1.0,  0.0) * kern).a;
    accum += texture(texture0, uv + vec2( 1.0,  0.0) * kern).a;
    accum += texture(texture0, uv + vec2( 0.0, -1.0) * kern).a;
    accum += texture(texture0, uv + vec2( 0.0,  1.0) * kern).a;
    accum += texture(texture0, uv + vec2(-1.0, -1.0) * kern).a;
    accum += texture(texture0, uv + vec2( 1.0, -1.0) * kern).a;
    accum += texture(texture0, uv + vec2(-1.0,  1.0) * kern).a;
    accum += texture(texture0, uv + vec2( 1.0,  1.0) * kern).a;

    if (accum <= 0.0) {
        discard;
    }

    float alpha = clamp(accum / 2.0, 0.0, 1.0);
    fragColor = vec4(0.05, 0.05, 0.05, alpha);
}
