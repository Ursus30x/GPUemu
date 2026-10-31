#version 450 core

layout(location = 0) in vec3 aPos;

void main()
{
    float sum = 0.0;
    for (int i = 0; i < 3; ++i) {
        if (aPos.y > 0.0) {
            sum += aPos.x;
        } else {
            sum += aPos.z;
        }
    }
    gl_Position = vec4(sum, aPos.y, aPos.z, 1.0);
}
