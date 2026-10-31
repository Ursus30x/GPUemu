#version 450 core

layout(location = 0) in vec3 aPos;

void main()
{
    vec4 outPos = vec4(aPos, 1.0);
    if (aPos.x > 0.0) {
        outPos.x += 1.0;
    }
    gl_Position = outPos;
}
