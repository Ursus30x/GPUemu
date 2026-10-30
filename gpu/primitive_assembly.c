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

static inline uint8_t mesh_color_to_u8(float c)
{
    if (c <= 0.0f) return 0;
    float scaled = c * 255.0f;
    if (scaled >= 255.0f) return 255;
    return (uint8_t)(scaled + 0.5f);
}

static inline bool mesh_vec4_is_nonzero(const float v[4])
{
    return v[0] != 0.0f || v[1] != 0.0f || v[2] != 0.0f || v[3] != 0.0f;
}

static void populate_mesh_vertex(TransformedVertex *dst, const BuiltinMeshOutput *mo, uint32_t vi, uint32_t pi)
{
    dst->pos.x = mo->vertices[vi].position[0];
    dst->pos.y = mo->vertices[vi].position[1];
    dst->pos.z = mo->vertices[vi].position[2];
    dst->pos.w = mo->vertices[vi].position[3];

    const float *v_var0 = mo->per_vertex_out[0][vi];
    const float *v_var1 = mo->per_vertex_out[1][vi];
    const float *v_var2 = mo->per_vertex_out[2][vi];

    const float *p_var0 = mo->per_primitive_out[0][pi];
    const float *p_var1 = mo->per_primitive_out[1][pi];
    const float *p_var2 = mo->per_primitive_out[2][pi];

    const float *loc0 = mesh_vec4_is_nonzero(p_var0) ? p_var0 : v_var0;
    const float *loc1 = mesh_vec4_is_nonzero(p_var1) ? p_var1 : v_var1;
    const float *loc2 = mesh_vec4_is_nonzero(p_var2) ? p_var2 : v_var2;

    if (mesh_vec4_is_nonzero(loc0)) {
        uint8_t cr = mesh_color_to_u8(loc0[0]);
        uint8_t cg = mesh_color_to_u8(loc0[1]);
        uint8_t cb = mesh_color_to_u8(loc0[2]);
        uint8_t ca = (loc0[3] > 0.0f) ? mesh_color_to_u8(loc0[3]) : 255;
        dst->color = RGBA_TO_UINT(cr, cg, cb, ca);
    } else {
        dst->color = 0xFFFFFFFFu;
    }

    if (mesh_vec4_is_nonzero(loc1)) {
        dst->u = loc1[0];
        dst->v = loc1[1];
    } else {
        dst->u = loc0[0];
        dst->v = loc0[1];
    }

    if (mesh_vec4_is_nonzero(loc2)) {
        dst->normal.x = loc2[0];
        dst->normal.y = loc2[1];
        dst->normal.z = loc2[2];
    } else {
        dst->normal.x = 0.0f;
        dst->normal.y = 0.0f;
        dst->normal.z = 1.0f;
    }
}

void assemble_mesh_primitives(
    GpuState *gpu,
    const BuiltinMeshOutput *mesh_outputs,
    uint32_t num_mesh_workgroups,
    TransformedVertex **out_vertices,
    Triangle **out_triangles,
    Edge **out_lines,
    uint32_t *out_prim_count,
    GpuPrimitiveType *out_prim_type)
{
    *out_vertices = NULL;
    *out_triangles = NULL;
    *out_lines = NULL;
    *out_prim_count = 0;

    GpuPrimitiveType prim_type = GPU_PRIM_TRIANGLES;
    if (gpu->ms_output_topology == SpvExecutionModeOutputPoints) {
        prim_type = GPU_PRIM_POINTS;
    } else if (gpu->ms_output_topology == SpvExecutionModeOutputLinesEXT) {
        prim_type = GPU_PRIM_LINES;
    }
    *out_prim_type = prim_type;

    if (!mesh_outputs || num_mesh_workgroups == 0) {
        return;
    }

    uint32_t total_prims = 0;
    for (uint32_t mwg = 0; mwg < num_mesh_workgroups; mwg++) {
        const BuiltinMeshOutput *mo = &mesh_outputs[mwg];
        uint32_t v_cnt = mo->vertex_count;
        if (v_cnt > MESH_MAX_VERTICES) v_cnt = MESH_MAX_VERTICES;
        if (gpu->ms_max_vertices > 0 && v_cnt > gpu->ms_max_vertices) v_cnt = gpu->ms_max_vertices;

        uint32_t p_cnt = mo->primitive_count;
        if (p_cnt > MESH_MAX_PRIMITIVES) p_cnt = MESH_MAX_PRIMITIVES;
        if (gpu->ms_max_primitives > 0 && p_cnt > gpu->ms_max_primitives) p_cnt = gpu->ms_max_primitives;

        if (v_cnt == 0 || p_cnt == 0) continue;

        for (uint32_t p = 0; p < p_cnt; p++) {
            if (mo->primitives[p].cull_primitive != 0) continue;

            if (prim_type == GPU_PRIM_POINTS) {
                if (mo->point_indices[p] < v_cnt) total_prims++;
            } else if (prim_type == GPU_PRIM_LINES) {
                if (mo->line_indices[p][0] < v_cnt && mo->line_indices[p][1] < v_cnt) total_prims++;
            } else {
                if (mo->triangle_indices[p][0] < v_cnt &&
                    mo->triangle_indices[p][1] < v_cnt &&
                    mo->triangle_indices[p][2] < v_cnt) {
                    total_prims++;
                }
            }
        }
    }

    if (total_prims == 0) {
        return;
    }

    uint32_t verts_per_prim = (prim_type == GPU_PRIM_POINTS) ? 1 : ((prim_type == GPU_PRIM_LINES) ? 2 : 3);
    TransformedVertex *verts = (TransformedVertex *)malloc(sizeof(TransformedVertex) * total_prims * verts_per_prim);
    if (!verts) {
        return;
    }

    Triangle *tris = NULL;
    Edge *lines = NULL;
    if (prim_type == GPU_PRIM_TRIANGLES) {
        tris = (Triangle *)malloc(sizeof(Triangle) * total_prims);
        if (!tris) {
            free(verts);
            return;
        }
    } else if (prim_type == GPU_PRIM_LINES) {
        lines = (Edge *)malloc(sizeof(Edge) * total_prims);
        if (!lines) {
            free(verts);
            return;
        }
    }

    uint32_t prim_idx = 0;
    for (uint32_t mwg = 0; mwg < num_mesh_workgroups; mwg++) {
        const BuiltinMeshOutput *mo = &mesh_outputs[mwg];
        uint32_t v_cnt = mo->vertex_count;
        if (v_cnt > MESH_MAX_VERTICES) v_cnt = MESH_MAX_VERTICES;
        if (gpu->ms_max_vertices > 0 && v_cnt > gpu->ms_max_vertices) v_cnt = gpu->ms_max_vertices;

        uint32_t p_cnt = mo->primitive_count;
        if (p_cnt > MESH_MAX_PRIMITIVES) p_cnt = MESH_MAX_PRIMITIVES;
        if (gpu->ms_max_primitives > 0 && p_cnt > gpu->ms_max_primitives) p_cnt = gpu->ms_max_primitives;

        if (v_cnt == 0 || p_cnt == 0) continue;

        for (uint32_t p = 0; p < p_cnt; p++) {
            if (mo->primitives[p].cull_primitive != 0) continue;

            if (prim_type == GPU_PRIM_POINTS) {
                uint32_t i0 = mo->point_indices[p];
                if (i0 < v_cnt) {
                    populate_mesh_vertex(&verts[prim_idx], mo, i0, p);
                    prim_idx++;
                }
            } else if (prim_type == GPU_PRIM_LINES) {
                uint32_t i0 = mo->line_indices[p][0];
                uint32_t i1 = mo->line_indices[p][1];
                if (i0 < v_cnt && i1 < v_cnt) {
                    populate_mesh_vertex(&verts[2 * prim_idx + 0], mo, i0, p);
                    populate_mesh_vertex(&verts[2 * prim_idx + 1], mo, i1, p);
                    lines[prim_idx].a = 2 * prim_idx + 0;
                    lines[prim_idx].b = 2 * prim_idx + 1;
                    prim_idx++;
                }
            } else {
                uint32_t i0 = mo->triangle_indices[p][0];
                uint32_t i1 = mo->triangle_indices[p][1];
                uint32_t i2 = mo->triangle_indices[p][2];
                if (i0 < v_cnt && i1 < v_cnt && i2 < v_cnt) {
                    populate_mesh_vertex(&verts[3 * prim_idx + 0], mo, i0, p);
                    populate_mesh_vertex(&verts[3 * prim_idx + 1], mo, i1, p);
                    populate_mesh_vertex(&verts[3 * prim_idx + 2], mo, i2, p);
                    tris[prim_idx].a = 3 * prim_idx + 0;
                    tris[prim_idx].b = 3 * prim_idx + 1;
                    tris[prim_idx].c = 3 * prim_idx + 2;
                    prim_idx++;
                }
            }
        }
    }

    *out_vertices = verts;
    *out_triangles = tris;
    *out_lines = lines;
    *out_prim_count = prim_idx;
}
