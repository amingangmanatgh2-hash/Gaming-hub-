#version 150
// ShadowCore BW — Fragment: bright, beautiful, light
// Slight brightness + cyan tint for BedWars clarity, no performance cost
in vec2 texcoord;
in vec4 glcolor;

uniform sampler2D gtexture;

out vec4 fragColor;

void main() {
    vec4 color = texture(gtexture, texcoord) * glcolor;

    // Brightness boost — like Fullbright but soft (BW dark corners visible)
    float brightness = 1.18;
    color.rgb *= brightness;

    // Gentle cyan tint matching ShadowCore theme (0.0 = original, 0.08 = tint)
    vec3 bwTint = vec3(0.88, 1.02, 1.06);
    color.rgb = mix(color.rgb, color.rgb * bwTint, 0.08);

    // Light contrast boost — beautiful on wool/bed colors
    color.rgb = pow(color.rgb, vec3(0.97));

    // Clamp to avoid washout
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    fragColor = color;
}
