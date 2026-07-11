#ifndef GPU_ASM
#define GPU_ASM
#include "gpu.h"
uint8_t cmp_u32(uint32_t a, uint32_t b, uint8_t flag);
uint8_t cmp_f32(float a, float b, uint8_t flag);
void exec_shader(GpuState *gpu, uint32_t program_offset);
#endif