#include "primitive_assembly.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

uint32_t fetch_primitive_index(GpuState *gpu, uint32_t pos)
{
    if (gpu->edge_config.size > 0 && gpu->edge_config.addr != 0) 
    {
        uint32_t *raw_indices = (uint32_t *)(gpu->vram_ptr + gpu->edge_config.addr);
        return raw_indices[pos];
    }
    return pos;
}

Triangle *assemble_primitive_triangles(GpuState *gpu, GpuPrimitiveType prim_type, uint32_t vertex_count, uint32_t *out_count)
{
    uint32_t num_indices = (gpu->edge_config.size > 0) ? (gpu->edge_config.size * 3) : vertex_count;
    Triangle *assembled = NULL;
    uint32_t tri_count = 0;

    switch (prim_type) 
    {
        case GPU_PRIM_TRIANGLES: 
        {
            if (gpu->edge_config.size > 0) 
            {
                tri_count = gpu->edge_config.size;
                assembled = malloc(sizeof(Triangle) * tri_count);
                if (assembled) 
                {
                    Triangle *src = TRIANGLES_TABLE(gpu);
                    memcpy(assembled, src, sizeof(Triangle) * tri_count);
                }
            } 
            else 
            {
                tri_count = vertex_count / 3;
                if (tri_count > 0) 
                {
                    assembled = malloc(sizeof(Triangle) * tri_count);
                    if (assembled) 
                    {
                        for (uint32_t i = 0; i < tri_count; i++) 
                        {
                            assembled[i].a = 3 * i;
                            assembled[i].b = 3 * i + 1;
                            assembled[i].c = 3 * i + 2;
                        }
                    }
                }
            }
            break;
        }

        case GPU_PRIM_TRIANGLE_STRIP:
        {
            if (num_indices >= 3) 
            {
                tri_count = num_indices - 2;
                assembled = malloc(sizeof(Triangle) * tri_count);
                if (assembled) 
                {
                    for (uint32_t i = 0; i < tri_count; i++) 
                    {
                        if (i % 2 == 0) 
                        {
                            assembled[i].a = fetch_primitive_index(gpu, i);
                            assembled[i].b = fetch_primitive_index(gpu, i + 1);
                            assembled[i].c = fetch_primitive_index(gpu, i + 2);
                        } else {
                            assembled[i].a = fetch_primitive_index(gpu, i + 1);
                            assembled[i].b = fetch_primitive_index(gpu, i);
                            assembled[i].c = fetch_primitive_index(gpu, i + 2);
                        }
                    }
                }
            }
            break;
        }

        case GPU_PRIM_TRIANGLE_FAN: {
            if (num_indices >= 3) 
            {
                tri_count = num_indices - 2;
                assembled = malloc(sizeof(Triangle) * tri_count);
                if (assembled) 
                {
                    uint32_t v0 = fetch_primitive_index(gpu, 0);
                    for (uint32_t i = 0; i < tri_count; i++) 
                    {
                        assembled[i].a = v0;
                        assembled[i].b = fetch_primitive_index(gpu, i + 1);
                        assembled[i].c = fetch_primitive_index(gpu, i + 2);
                    }
                }
            }
            break;
        }

        case GPU_PRIM_QUADS: {
            uint32_t quad_count = num_indices / 4;
            if (quad_count > 0) 
            {
                tri_count = quad_count * 2;
                assembled = malloc(sizeof(Triangle) * tri_count);
                if (assembled) 
                {
                    for (uint32_t q = 0; q < quad_count; q++) 
                    {
                        uint32_t idx = 4 * q;
                        uint32_t v0 = fetch_primitive_index(gpu, idx);
                        uint32_t v1 = fetch_primitive_index(gpu, idx + 1);
                        uint32_t v2 = fetch_primitive_index(gpu, idx + 2);
                        uint32_t v3 = fetch_primitive_index(gpu, idx + 3);

                        assembled[2 * q].a = v0;
                        assembled[2 * q].b = v1;
                        assembled[2 * q].c = v2;

                        assembled[2 * q + 1].a = v0;
                        assembled[2 * q + 1].b = v2;
                        assembled[2 * q + 1].c = v3;
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    *out_count = tri_count;
    return assembled;
}

Edge *assemble_primitive_lines(GpuState *gpu, GpuPrimitiveType prim_type, uint32_t vertex_count, uint32_t *out_count)
{
    Edge *assembled = NULL;
    uint32_t line_count = 0;

    if (prim_type == GPU_PRIM_LINES) 
    {
        if (gpu->edge_config.size > 0) 
        {
            line_count = gpu->edge_config.size;
            assembled = malloc(sizeof(Edge) * line_count);
            if (assembled) 
            {
                Edge *src = EDGES_TABLE(gpu);
                memcpy(assembled, src, sizeof(Edge) * line_count);
            }
        } 
        else 
        {
            line_count = vertex_count / 2;
            if (line_count > 0) 
            {
                assembled = malloc(sizeof(Edge) * line_count);
                if (assembled) 
                {
                    for (uint32_t i = 0; i < line_count; i++) 
                    {
                        assembled[i].a = 2 * i;
                        assembled[i].b = 2 * i + 1;
                    }
                }
            }
        }
    } 
    else if (prim_type == GPU_PRIM_LINE_STRIP) 
    {
        uint32_t num_indices = (gpu->edge_config.size > 0) ? (gpu->edge_config.size * 2) : vertex_count;
        if (num_indices >= 2) 
        {
            line_count = num_indices - 1;
            assembled = malloc(sizeof(Edge) * line_count);
            if (assembled) 
            {
                for (uint32_t i = 0; i < line_count; i++) 
                {
                    assembled[i].a = fetch_primitive_index(gpu, i);
                    assembled[i].b = fetch_primitive_index(gpu, i + 1);
                }
            }
        }
    }

    *out_count = line_count;
    return assembled;
}
