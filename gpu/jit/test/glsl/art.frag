#version 450

layout(location = 0) out vec4 outColor;

// Time uniform
layout(set = 0, binding = 0) uniform Uniforms
{
    float time;
} u;

void main()
{
    // Fixed resolution: 640x480
    vec2 uv = gl_FragCoord.xy / vec2(640.0, 480.0);
    uv = uv * 2.0 - 1.0;

    // Aspect correction (640 / 480 = 4/3)
    uv.x *= 640.0 / 480.0;

    // Polar coordinates
    float angle = atan(uv.y, uv.x);
    float radius = length(uv);

    // Animation
    float wave = sin(radius * 10.0 - u.time * 2.0);

    float pattern = wave + cos(angle * 10.0 + u.time * 3.0);

    vec3 color = vec3(
        tan(pattern + u.time),
        tan(pattern - u.time * 0.5),
        tan(u.time * 0.3 + pattern * 0.7)
    );

    // Map from [-1,1] to [0,1]
    color = color * 0.5 + 0.5;

    // Radial fade
    color *= exp(-radius * 3.0);

    outColor = vec4(color, 1.0);
}