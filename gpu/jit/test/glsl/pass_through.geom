#version 450
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec4 inColor[];
layout (location = 0) out vec4 outColor;

void main() {
    for (int i = 0; i < gl_in.length(); i++) {
        gl_Position = gl_in[i].gl_Position;
        outColor = inColor[i];
        EmitVertex();
    }
    EndPrimitive();
}
