#ifndef PRIMITIVE_ASSEMBLY_H
#define PRIMITIVE_ASSEMBLY_H

#include "renderer.h"

uint32_t fetch_primitive_index(GpuState *gpu, uint32_t pos);

Triangle *assemble_primitive_triangles(GpuState *gpu, GpuPrimitiveType prim_type, uint32_t vertex_count, uint32_t *out_count);

Edge *assemble_primitive_lines(GpuState *gpu, GpuPrimitiveType prim_type, uint32_t vertex_count, uint32_t *out_count);

#endif
