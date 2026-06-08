#version 300 es
precision mediump float;
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    finalColor = texelColor * colDiffuse * fragColor;
    if (finalColor.a < 0.01) discard;
    gl_FragDepth = gl_FragCoord.z - 0.001;
}
