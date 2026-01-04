#include "gpu.h"
#ifndef MATH3D
#define MATH3D
Vec4 vec4_add(Vec4 a, Vec4 b);
Mat4 mat4_mul(Mat4 *a, Mat4 *b);
Mat4 mat4_identity(void);
Mat4 mat4_rotate_y(float angle);
Mat4 mat4_rotate_x(float angle);
Mat4 mat4_translate(float x, float y, float z);
Mat4 mat4_perspective(float fov, float aspect, float near, float far);
Vec4 mat4_mul_vec4(Mat4 *mat, Vec4 v);
Mat4 get_mat_from_arg(int arg_val, Mat4* gpu_regs, uint8_t* shader_segment);
Mat4 mat4_scale(float sx, float sy, float sz);
Mat4 mat4_scale_uniform(float s);
void print_mat4(const Mat4* mat, const char* name);
#endif