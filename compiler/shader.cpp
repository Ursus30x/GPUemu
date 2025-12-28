void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    uv = uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    float p0 = iTime; // t
    float p1 = length(uv); // r
    float p2 = atan(uv.y, uv.x); // a
    float p3 = 10.0 * p1;
    float p4 = p0 * 2.0;
    p3 = p3 - p4;
    p3 = sin(p3);
    p4 = 10.0 * p2;
    float p5 = p0 * 3.0;
    p5 = p4 + p5;
    p5 = cos(p5);
    p5 = p3 + p5;
    float p6 = p5 + p0;
    p6 = tan(p6);
    float p7 = p0*0.5;
    p7 = p5 - p7;
    p7 = tan(p7);
    p3 = p0*0.3;
    p5 = p5*0.7;
    float p8 = p3 + p5;
    p8 = tan(p8);
    vec3 m0 = vec3(p6,p7,p8);
    m0 = 0.5 * m0;
    m0 = 0.5 + m0;
    p2 = -p1 * 3.0;
    p2 = exp(p2);
    m0 = m0 * p2;
    fragColor = vec4(m0, 1.0);
}
