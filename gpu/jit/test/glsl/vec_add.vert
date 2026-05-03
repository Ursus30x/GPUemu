#version 450

layout(location = 0) out float result;

layout(set = 0, binding = 0) uniform Params {
    vec3 x;
    vec3 y;
} ubo;

void main() {
    vec3 r = ubo.x + ubo.y;
    result = r.x;
}