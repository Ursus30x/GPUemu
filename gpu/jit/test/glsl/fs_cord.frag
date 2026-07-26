#version 450

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 uv = gl_FragCoord.xy;
    outColor = vec4(uv.x, uv.y, 0.0, 0.0);
}