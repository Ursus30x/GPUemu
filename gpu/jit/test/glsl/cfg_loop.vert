#version 450 core

layout(location = 0) in vec3 aPos;

void main()
{
    float sum = 0.0;
    for (int i = 0; i < 3; ++i) {
        sum += aPos.x;
    }
    gl_Position = vec4(sum, aPos.y, aPos.z, 1.0);
}
