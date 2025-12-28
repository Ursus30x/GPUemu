#include "math3d.h"

Mat4 mat4_mul(Mat4 *a, Mat4 *b) 
{
    Mat4 r = {0};
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            for(int k=0;k<4;k++)
                r.m[i][j] += a->m[i][k] * b->m[k][j];
    return r;
}

Mat4 mat4_identity(void) 
{
    Mat4 m = {0};
    for(int i=0;i<4;i++) m.m[i][i] = 1.0f;
    return m;
}

Mat4 mat4_rotate_y(float angle) 
{
    Mat4 m = mat4_identity();
    m.m[0][0] = cosf(angle);  m.m[0][2] = sinf(angle);
    m.m[2][0] = -sinf(angle); m.m[2][2] = cosf(angle);
    return m;
}

Mat4 mat4_rotate_x(float angle) 
{
    Mat4 m = mat4_identity();
    m.m[1][1] = cosf(angle);  m.m[1][2] = -sinf(angle);
    m.m[2][1] = sinf(angle);  m.m[2][2] = cosf(angle);
    return m;
}

Mat4 mat4_translate(float x, float y, float z) 
{
    Mat4 m = mat4_identity();
    m.m[0][3] = x; m.m[1][3] = y; m.m[2][3] = z;
    return m;
}

Mat4 mat4_perspective(float fov, float aspect, float near, float far) 
{
    Mat4 m = {0};
    float f = 1.0f / tanf(fov * 0.5f);
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = far / (far - near);
    m.m[2][3] = (-far * near) / (far - near);
    m.m[3][2] = 1.0f;
    return m;
}

Vec4 mat4_mul_vec4(Mat4 *mat, Vec4 v)
{
    Vec4 r;
    r.x = mat->m[0][0]*v.x + mat->m[0][1]*v.y + mat->m[0][2]*v.z + mat->m[0][3]*v.w;
    r.y = mat->m[1][0]*v.x + mat->m[1][1]*v.y + mat->m[1][2]*v.z + mat->m[1][3]*v.w;
    r.z = mat->m[2][0]*v.x + mat->m[2][1]*v.y + mat->m[2][2]*v.z + mat->m[2][3]*v.w;
    r.w = mat->m[3][0]*v.x + mat->m[3][1]*v.y + mat->m[3][2]*v.z + mat->m[3][3]*v.w;
    return r;
}

Mat4 get_mat_from_arg(int arg_val, Mat4* gpu_regs, uint8_t* shader_segment) 
{
    Mat4 mat;
    if (0) {
        memcpy(&mat, shader_segment + arg_val, sizeof(Mat4));
    } else {
        uint32_t src = arg_val;
        mat = gpu_regs[src];
    }

    return mat;
}
Mat4 mat4_scale(float sx, float sy, float sz)
{
    Mat4 m = mat4_identity();
    m.m[0][0] = sx;
    m.m[1][1] = sy;
    m.m[2][2] = sz;
    return m;
}
Mat4 mat4_scale_uniform(float s)
{
    return mat4_scale(s, s, s);
}

void print_mat4(const Mat4* mat, const char* name)
{
    if (!mat) return;
    #ifdef DEBUG_MAT
    printf("Matrix %s:\n", name);
    for (int i = 0; i < 4; ++i) {
        printf("[ ");
        for (int j = 0; j < 4; ++j) {
            printf("%8.3f ", mat->m[i][j]);
        }
        printf("]\n");
    }
    printf("\n");
    #endif
}