#version 450


layout(set = 0, binding = 0) uniform Params {
    float x; // 2
    float y; // 4
} ubo;
void main() {
    vec4 v4 = vec4(ubo.x + ubo.y, ubo.x - ubo.y, ubo.x * ubo.y, ubo.x / ubo.y);
    gl_Position = v4;
}