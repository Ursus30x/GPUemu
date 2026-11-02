#version 450
layout(set = 0, binding = 0) uniform Params {
    vec4 v;
    ivec4 iv;
    uvec4 uv;
    mat4 m;
} ubo;

void main() {
    // 1. SAbs, SSign, SMin, SMax, SClamp
    ivec4 a = abs(ubo.iv);
    ivec4 s = sign(ubo.iv);
    ivec4 s_min = min(ubo.iv, ivec4(0));
    ivec4 s_max = max(ubo.iv, ivec4(0));
    ivec4 s_cl = clamp(ubo.iv, ivec4(-5), ivec4(5));
    ivec4 im = mix(ubo.iv, a, bvec4(true, false, true, false));

    // 2. UMin, UMax, UClamp
    uvec4 u_min = min(ubo.uv, uvec4(10u));
    uvec4 u_max = max(ubo.uv, uvec4(10u));
    uvec4 u_cl = clamp(ubo.uv, uvec4(2u), uvec4(8u));

    // 3. Bit scanning
    ivec4 lsb = findLSB(ubo.iv);
    ivec4 smsb = findMSB(ubo.iv);
    uvec4 umsb = uvec4(findMSB(ubo.uv));

    // 4. Matrix determinant and inverse
    float det = determinant(ubo.m);
    mat4 inv_m = inverse(ubo.m);

    // 5. Geometric faceforward
    vec4 ff = faceforward(ubo.v, vec4(1.0, 0.0, 0.0, 0.0), vec4(1.0, 0.0, 0.0, 0.0));

    // 6. Packing / Unpacking
    uint ph = packHalf2x16(ubo.v.xy);
    vec2 uph = unpackHalf2x16(ph);

    uint pu2 = packUnorm2x16(ubo.v.xy);
    vec2 upu2 = unpackUnorm2x16(pu2);

    uint ps2 = packSnorm2x16(ubo.v.xy);
    vec2 ups2 = unpackSnorm2x16(ps2);

    uint pu4 = packUnorm4x8(ubo.v);
    vec4 upu4 = unpackUnorm4x8(pu4);

    uint ps4 = packSnorm4x8(ubo.v);
    vec4 ups4 = unpackSnorm4x8(ps4);

    vec4 fa = vec4(float(a.x), float(a.y), float(a.z), float(a.w));
    vec4 fs = vec4(float(s.x), float(s.y), float(s.z), float(s.w));
    vec4 fmin = vec4(float(s_min.x), float(s_min.y), float(s_min.z), float(s_min.w));
    vec4 fmax = vec4(float(s_max.x), float(s_max.y), float(s_max.z), float(s_max.w));
    vec4 fcl = vec4(float(s_cl.x), float(s_cl.y), float(s_cl.z), float(s_cl.w));
    vec4 fim = vec4(float(im.x), float(im.y), float(im.z), float(im.w));

    vec4 fu_min = vec4(float(u_min.x), float(u_min.y), float(u_min.z), float(u_min.w));
    vec4 fu_max = vec4(float(u_max.x), float(u_max.y), float(u_max.z), float(u_max.w));
    vec4 fu_cl = vec4(float(u_cl.x), float(u_cl.y), float(u_cl.z), float(u_cl.w));

    vec4 flsb = vec4(float(lsb.x), float(lsb.y), float(lsb.z), float(lsb.w));
    vec4 fsmsb = vec4(float(smsb.x), float(smsb.y), float(smsb.z), float(smsb.w));
    vec4 fumsb = vec4(float(umsb.x), float(umsb.y), float(umsb.z), float(umsb.w));

    vec4 int_sum = fa + fs + fmin + fmax + fcl + fim + fu_min + fu_max + fu_cl + flsb + fsmsb + fumsb;
    vec4 pack_sum = vec4(uph, upu2) + vec4(ups2, 0.0, 0.0) + upu4 + ups4;
    vec4 mat_sum = inv_m[0] + inv_m[1] + inv_m[2] + inv_m[3] + vec4(det);

    gl_Position = int_sum + pack_sum + mat_sum + ff;
}
