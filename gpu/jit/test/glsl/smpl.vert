#version 450

layout(location = 0) in vec3 aPos;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
} ubo;


layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
void main()
{
    gl_Position = ubo.mvp * vec4(aPos, 1.0);
    fragTexCoord = inTexCoord;
}

