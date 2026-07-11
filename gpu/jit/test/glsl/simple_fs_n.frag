#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec3 outColor;

void main() {
    outColor = fragColor;
}