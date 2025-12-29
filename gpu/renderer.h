#ifndef RENDERER
#define RENDERER
#include "gpu.h"

#define GET_R(color) (uint8_t)(((color) >> 16) & 0xFF)
#define GET_G(color) (uint8_t)(((color) >> 8) & 0xFF)
#define GET_B(color) (uint8_t)((color) & 0xFF)

#define RGB_TO_UINT(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

void put_pixel(GpuState *gpu, int x, int y, uint32_t color);
void draw_line(GpuState *gpu, int x0, int y0, int x1, int y1, uint32_t color1, uint32_t color2);
void exec_shader(GpuState *gpu, uint32_t program_offset);

uint8_t cmp_u32(uint32_t a, uint32_t b, uint8_t flag);
uint8_t cmp_f32(float a, float b, uint8_t flag);
void gpu_render_wireframe(void *opaque);
void gpu_render_triangles(void *opaque);
float edge_func(Vec3 a, Vec3 b, Vec3 c);
void draw_triangle(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu);
#endif