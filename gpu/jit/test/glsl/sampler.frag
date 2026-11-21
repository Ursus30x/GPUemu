#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D uTexture;

void main() {
    fragColor = texture(uTexture, vTexCoord);
}