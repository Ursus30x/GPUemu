#version 450
layout(set = 0, binding = 0) uniform Params {
    vec4 v;
} ubo;
void main() {
    vec4 r = round(ubo.v);
    vec4 f = floor(ubo.v);
    vec4 c = clamp(ubo.v, vec4(0.0), vec4(5.0));

    ivec4 exp_val;
    vec4 sig = frexp(ubo.v, exp_val);
    vec4 reconstructed = ldexp(sig, exp_val);

    gl_Position = r + f + c + (reconstructed - ubo.v);
}
