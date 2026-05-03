#version 450

layout(location = 0) out float result;

layout(set = 0, binding = 0) uniform Params {
    float x;
    float y;
} ubo;

void main() {
    float x = 2 * ubo.x;
    result = ubo.x + ubo.y + x;
}