#version 450

layout(location = 0) out vec3 result;

layout(set = 0, binding = 0) uniform Params {
    vec3 x;
    vec3 y;
} ubo;

void main() {
    result = ubo.x + ubo.y;
}