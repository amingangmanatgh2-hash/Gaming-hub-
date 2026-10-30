#version 150
// ShadowCore BW — Vertex: pass-through with light tint
in vec3 vaPosition;
in vec2 vaUV0;
in vec4 vaColor;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform vec3 chunkOffset;

out vec2 texcoord;
out vec4 glcolor;

void main() {
    gl_Position = projectionMatrix * modelViewMatrix * vec4(vaPosition + chunkOffset, 1.0);
    texcoord = vaUV0;
    glcolor = vaColor;
}
