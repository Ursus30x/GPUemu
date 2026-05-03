#version 450

layout(location = 0) out float result;

layout(set = 0, binding = 0) uniform Params {
    float x;
    float y;
} ubo;

void main() {
    vec3 v3 = vec3(ubo.x + ubo.y, 0.0, 2.0);
    result = v3.x + v3.y + v3.z;
}