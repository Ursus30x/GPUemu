#include "gpu.h"
#ifndef MATH3D
#define MATH3d
Mat4 mat4_mul(Mat4 *a, Mat4 *b);
Mat4 mat4_identity(void);
Mat4 mat4_rotate_y(float angle);
Mat4 mat4_rotate_x(float angle);
Mat4 mat4_translate(float x, float y, float z);
Mat4 mat4_perspective(float fov, float aspect, float near, float far);
Vec4 mat4_mul_vec4(Mat4 *mat, Vec4 v);
Mat4 get_mat_from_arg(int arg_val, Mat4* gpu_regs, uint8_t* shader_segment);
void print_mat4(const Mat4* mat, const char* name);
#endif