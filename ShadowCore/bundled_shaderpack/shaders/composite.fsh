#version 150
// ShadowCore BW composite — very cheap bloom for beautiful light
in vec2 texcoord;
uniform sampler2D colortex0;
out vec4 fragColor;
void main() {
    vec4 c = texture(colortex0, texcoord);
    // subtle bloom on bright wool/bed
    float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    if (luma > 0.85) c.rgb += vec3(0.02);
    fragColor = c;
}
