#version 450

layout(location = 0) out float result;

layout(set = 0, binding = 0) uniform Params {
    float x;
    float y;
} ubo;

void main() {
    vec3 b = vec3(ubo.x, 0.0, 1.0);
    vec3 a = vec3(ubo.y, 0.0, 1.0);
    vec3 c = a + 2.0;
    c = c - b;
    vec3 d = c;
    c = c * 2.0;
    c = c - d;
    result = c.x + c.z;
}