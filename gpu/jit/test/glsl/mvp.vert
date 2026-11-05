#version 450 core

layout(location = 0) in vec3 aPos;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
} ubo;

void main()
{  
    gl_Position = ubo.mvp * vec4(aPos, 1.0);
}