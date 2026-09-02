#include "rasterizer_simt.h"
#include "math3d.h"
#include <math.h>

static inline uint8_t color_to_u8(float c)
{
    if (c <= 0.0f) return 0;
    float scaled = c * 255.0f;
    if (scaled >= 255.0f) return 255;
    return (uint8_t)(scaled + 0.5f);
}

static void bind_ubo_to_context(GpuState *gpu, ExecutionContext *ectx)
{
    if (gpu->uinform_config.size > 0 && gpu->uinform_config.addr != 0)
    {
        void *ubo_data = gpu->vram_ptr + gpu->uinform_config.addr;
        ectx->binding_buffers[0] = ubo_data;
    }
}

static void bind_resources_to_context(GpuState *gpu, ExecutionContext *ectx)
{
    bind_ubo_to_context(gpu, ectx);
    for (int slot = 1; slot < MAX_BINDINGS; slot++)
    {
        if (gpu->texture_desc_addr[slot] != 0 && gpu->texture_desc_addr[slot] + sizeof(GpuTextureDescriptorVram) <= GPU_VRAM_SIZE)
        {
            GpuTextureDescriptorVram *vram_desc = (GpuTextureDescriptorVram *)(gpu->vram_ptr + gpu->texture_desc_addr[slot]);
            if (vram_desc->data_vram_addr != 0 && vram_desc->data_vram_addr < GPU_VRAM_SIZE)
            {
                gpu->textures[slot].data = (void *)(gpu->vram_ptr + vram_desc->data_vram_addr);
                gpu->textures[slot].width = vram_desc->width;
                gpu->textures[slot].height = vram_desc->height;
                gpu->textures[slot].channels = vram_desc->channels;
                gpu->textures[slot].filter = (FilterMode)vram_desc->filter;
                gpu->textures[slot].wrap = (WrapMode)vram_desc->wrap;
                ectx->binding_buffers[slot] = &gpu->textures[slot];
            }
            else
            {
                ectx->binding_buffers[slot] = NULL;
            }
        }
        else
        {
            ectx->binding_buffers[slot] = NULL;
        }
    }
}

void worker_transform_vertices_simt_impl(RenderThreadArgs *args) 
{
    GpuState local_gpu = *(args->orig_gpu); 
    GpuState *gpu = &local_gpu;

    Vec3 *vertices = VERTEX_TABLE(gpu);
    uint32_t total_vertices = gpu->vbo_config.size;

    for (uint32_t b = args->start_block; b < args->end_block; b++)
    {
        uint32_t base_idx = b * SIMT_WIDTH;

        uint16_t exec_mask = 0xFFFF;
        if (base_idx + SIMT_WIDTH > total_vertices) 
        {
            uint32_t active_lanes = total_vertices - base_idx;
            exec_mask = (1 << active_lanes) - 1;
        }

        SimtVec3 in_vec;
        SimtVec2 in_uv;
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            if (exec_mask & (1 << lane))
            {
                uint32_t i = base_idx + lane;
                in_vec.elem[0][lane] = vertices[i].x;
                in_vec.elem[1][lane] = vertices[i].y;
                in_vec.elem[2][lane] = vertices[i].z;

                in_uv.elem[0][lane]  = vertices[i].u;
                in_uv.elem[1][lane]  = vertices[i].v;
            }
            else
            {
                in_vec.elem[0][lane] = 0.0f;
                in_vec.elem[1][lane] = 0.0f;
                in_vec.elem[2][lane] = 0.0f;
                in_uv.elem[0][lane] = 0.0f;
                in_uv.elem[1][lane] = 0.0f;
            }
        }

        ExecutionContext jit_ctx = {0};
        BuiltinVertexOutput vs_out = {0};
        bind_resources_to_context(gpu, &jit_ctx);
        jit_ctx.location_in_buffers[0] = &in_vec;
        jit_ctx.location_in_buffers[1] = &in_uv;
        jit_ctx.location_out_buffers[0] = &args->transformed_uv_simt;
        gpu->vs_shader_func(&jit_ctx, &vs_out, NULL, NULL);
        args->transformed_simt = vs_out.gl_Position;

        for (uint32_t i = 0; i < SIMT_WIDTH; i++)
        {
            uint32_t global_idx = base_idx + i;
            if (global_idx < total_vertices)
            {
                args->transformed_vertices[global_idx].pos.x = args->transformed_simt.elem[0][i];
                args->transformed_vertices[global_idx].pos.y = args->transformed_simt.elem[1][i];
                args->transformed_vertices[global_idx].pos.z = args->transformed_simt.elem[2][i];
                args->transformed_vertices[global_idx].pos.w = args->transformed_simt.elem[3][i];
                args->transformed_vertices[global_idx].color = vertices[global_idx].rgba;
                args->transformed_vertices[global_idx].u     = args->transformed_uv_simt.elem[0][i];
                args->transformed_vertices[global_idx].v     = args->transformed_uv_simt.elem[1][i];
            }
        }
    }
}

static bool setup_geometry_and_bounds(Vec4 v0, Vec4 v1, Vec4 v2, GpuState *gpu, int band_min_y, int band_max_y, TriangleContext *ctx, float *out_inv_area) 
{
    Vec4 v[3] = {v0, v1, v2};
    uint32_t width = gpu->width;
    uint32_t height = gpu->height;

    for (int i = 0; i < 3; i++) 
    {
        float w = (v[i].w < 0.1f) ? 0.1f : v[i].w;
        float inv_w = 1.0f / w;

        ctx->s_inv_w[i] = inv_w;
        ctx->inv_z[i] = v[i].z * inv_w;

        float ndc_x = v[i].x * inv_w;
        float ndc_y = v[i].y * inv_w;

        ctx->s[i].x = (ndc_x + 1.0f) * 0.5f * width;
        ctx->s[i].y = (1.0f - (ndc_y + 1.0f) * 0.5f) * height;
        ctx->s[i].z = inv_w; 
    }

    float area = edge_func(ctx->s[0], ctx->s[1], ctx->s[2]);
    if (area == 0.0f) return false; 
    
    *out_inv_area = 1.0f / area;

    ctx->min_x = fmax(0, fmin(ctx->s[0].x, fmin(ctx->s[1].x, ctx->s[2].x)));
    ctx->max_x = fmin(width - 1, fmax(ctx->s[0].x, fmax(ctx->s[1].x, ctx->s[2].x)));
    
    ctx->min_y = fmax(band_min_y, fmin(ctx->s[0].y, fmin(ctx->s[1].y, ctx->s[2].y)));
    ctx->max_y = fmin(band_max_y, fmax(ctx->s[0].y, fmax(ctx->s[1].y, ctx->s[2].y)));

    if (ctx->min_y > ctx->max_y || ctx->min_x > ctx->max_x) return false;

    ctx->stamp_min_x = ctx->min_x & ~3;
    ctx->stamp_max_x = (ctx->max_x + 4) & ~3;
    ctx->stamp_min_y = ctx->min_y & ~3;
    ctx->stamp_max_y = (ctx->max_y + 4) & ~3;

    return true;
}

static void setup_invariants(Col3 color, float u[3], float v[3], float inv_area, TriangleContext *ctx) 
{
    Vec3 p_zero = {0.0f, 0.0f, 0.0f};
    Vec3 p_dx1  = {1.0f, 0.0f, 0.0f};
    Vec3 p_dy1  = {0.0f, 1.0f, 0.0f};

    float base0 = edge_func(ctx->s[1], ctx->s[2], p_zero);
    ctx->d_w0_dx = (edge_func(ctx->s[1], ctx->s[2], p_dx1) - base0) * inv_area;
    ctx->d_w0_dy = (edge_func(ctx->s[1], ctx->s[2], p_dy1) - base0) * inv_area;

    float base1 = edge_func(ctx->s[2], ctx->s[0], p_zero);
    ctx->d_w1_dx = (edge_func(ctx->s[2], ctx->s[0], p_dx1) - base1) * inv_area;
    ctx->d_w1_dy = (edge_func(ctx->s[2], ctx->s[0], p_dy1) - base1) * inv_area;

    float base2 = edge_func(ctx->s[0], ctx->s[1], p_zero);
    ctx->d_w2_dx = (edge_func(ctx->s[0], ctx->s[1], p_dx1) - base2) * inv_area;
    ctx->d_w2_dy = (edge_func(ctx->s[0], ctx->s[1], p_dy1) - base2) * inv_area;

    const float inv_255 = 1.0f / 255.0f;
    ctx->r_inv_w[0] = (GET_R(color.a_col) * inv_255) * ctx->s_inv_w[0];
    ctx->g_inv_w[0] = (GET_G(color.a_col) * inv_255) * ctx->s_inv_w[0];
    ctx->b_inv_w[0] = (GET_B(color.a_col) * inv_255) * ctx->s_inv_w[0];
    ctx->a_inv_w[0] = (GET_A(color.a_col) * inv_255) * ctx->s_inv_w[0];

    ctx->r_inv_w[1] = (GET_R(color.b_col) * inv_255) * ctx->s_inv_w[1];
    ctx->g_inv_w[1] = (GET_G(color.b_col) * inv_255) * ctx->s_inv_w[1];
    ctx->b_inv_w[1] = (GET_B(color.b_col) * inv_255) * ctx->s_inv_w[1];
    ctx->a_inv_w[1] = (GET_A(color.b_col) * inv_255) * ctx->s_inv_w[1];

    ctx->r_inv_w[2] = (GET_R(color.c_col) * inv_255) * ctx->s_inv_w[2];
    ctx->g_inv_w[2] = (GET_G(color.c_col) * inv_255) * ctx->s_inv_w[2];
    ctx->b_inv_w[2] = (GET_B(color.c_col) * inv_255) * ctx->s_inv_w[2];
    ctx->a_inv_w[2] = (GET_A(color.c_col) * inv_255) * ctx->s_inv_w[2];

    ctx->u_inv_w[0] = u[0] * ctx->s_inv_w[0];
    ctx->u_inv_w[1] = u[1] * ctx->s_inv_w[1];
    ctx->u_inv_w[2] = u[2] * ctx->s_inv_w[2];

    ctx->v_inv_w[0] = v[0] * ctx->s_inv_w[0];
    ctx->v_inv_w[1] = v[1] * ctx->s_inv_w[1];
    ctx->v_inv_w[2] = v[2] * ctx->s_inv_w[2];

    ctx->d_w3_dx = ctx->d_w0_dx * ctx->a_inv_w[0] + ctx->d_w1_dx * ctx->a_inv_w[1] + ctx->d_w2_dx * ctx->a_inv_w[2];
    ctx->d_w3_dy = ctx->d_w0_dy * ctx->a_inv_w[0] + ctx->d_w1_dy * ctx->a_inv_w[1] + ctx->d_w2_dy * ctx->a_inv_w[2];

    Vec3 start_p = {ctx->stamp_min_x + 0.5f, ctx->stamp_min_y + 0.5f, 0};
    ctx->start_w0 = edge_func(ctx->s[1], ctx->s[2], start_p) * inv_area;
    ctx->start_w1 = edge_func(ctx->s[2], ctx->s[0], start_p) * inv_area;
    ctx->start_w2 = edge_func(ctx->s[0], ctx->s[1], start_p) * inv_area;
    ctx->start_w3 = ctx->start_w0 * ctx->a_inv_w[0] + ctx->start_w1 * ctx->a_inv_w[1] + ctx->start_w2 * ctx->a_inv_w[2];
}

static uint16_t evaluate_stamp_coverage(int x, int y, float stamp_w0, float stamp_w1, float stamp_w2, float stamp_w3, TriangleContext *ctx, float *lane_w0, float *lane_w1, float *lane_w2, float *lane_w3) 
{
    uint16_t exec_mask = 0x0000;

    for (int lane = 0; lane < 16; lane++) 
    {
        int lx = lane % 4;
        int ly = lane / 4;
        int px = x + lx;
        int py = y + ly;

        float w0 = stamp_w0 + (lx * ctx->d_w0_dx) + (ly * ctx->d_w0_dy);
        float w1 = stamp_w1 + (lx * ctx->d_w1_dx) + (ly * ctx->d_w1_dy);
        float w2 = stamp_w2 + (lx * ctx->d_w2_dx) + (ly * ctx->d_w2_dy);
        float w3 = stamp_w3 + (lx * ctx->d_w3_dx) + (ly * ctx->d_w3_dy);
        
        lane_w0[lane] = w0;
        lane_w1[lane] = w1;
        lane_w2[lane] = w2;
        lane_w3[lane] = w3;

        bool in_bounds = (px >= ctx->min_x && px <= ctx->max_x && py >= ctx->min_y && py <= ctx->max_y);
        bool in_tri = (w0 >= -1e-4f && w1 >= -1e-4f && w2 >= -1e-4f);
        
        if (in_bounds && in_tri) 
        {
            exec_mask |= (1 << lane);
        }
    }
    return exec_mask;
}

static uint16_t process_z_and_interpolate(int x, int y, uint16_t exec_mask, float *lane_w0, float *lane_w1, float *lane_w2, float *lane_a, TriangleContext *ctx, GpuState *gpu, SimtVec4 *fs_in_color, SimtVec2 *fs_in_uv) 
{
    uint16_t shade_mask = 0x0000;
    uint32_t width = gpu->width;

    for (int lane = 0; lane < 16; lane++) 
    {
        if (!(exec_mask & (1 << lane))) continue;

        int px = x + (lane % 4);
        int py = y + (lane / 4);
        int idx = py * width + px;

        float w0 = lane_w0[lane];
        float w1 = lane_w1[lane];
        float w2 = lane_w2[lane];

        float z = w0 * ctx->inv_z[0] + w1 * ctx->inv_z[1] + w2 * ctx->inv_z[2];
        
        if (z < Z_BUFFER(gpu)[idx]) 
        {
            if(gpu->depth_write_enable == 1)
            {
                Z_BUFFER(gpu)[idx] = z;
            }
            shade_mask |= (1 << lane);

            float pixel_inv_w = w0 * ctx->s_inv_w[0] + w1 * ctx->s_inv_w[1] + w2 * ctx->s_inv_w[2];
            float pixel_w = 1.0f / pixel_inv_w;

            fs_in_color->elem[0][lane] = (w0 * ctx->r_inv_w[0] + w1 * ctx->r_inv_w[1] + w2 * ctx->r_inv_w[2]) * pixel_w;
            fs_in_color->elem[1][lane] = (w0 * ctx->g_inv_w[0] + w1 * ctx->g_inv_w[1] + w2 * ctx->g_inv_w[2]) * pixel_w;
            fs_in_color->elem[2][lane] = (w0 * ctx->b_inv_w[0] + w1 * ctx->b_inv_w[1] + w2 * ctx->b_inv_w[2]) * pixel_w;
            fs_in_color->elem[3][lane] = lane_a[lane];

            fs_in_uv->elem[0][lane] = (w0 * ctx->u_inv_w[0] + w1 * ctx->u_inv_w[1] + w2 * ctx->u_inv_w[2]) * pixel_w;
            fs_in_uv->elem[1][lane] = (w0 * ctx->v_inv_w[0] + w1 * ctx->v_inv_w[1] + w2 * ctx->v_inv_w[2]) * pixel_w;
        }
    }
    return shade_mask;
}

static void execute_shader_and_write(int x, int y, uint16_t shade_mask, GpuState *gpu, SimtVec4 *fs_in_color, SimtVec2 *fs_in_uv, BuiltinFragmentInput* fs_input) 
{
    SimtVec4 out_color = {0};
    ExecutionContext jit_ctx = {0};
    
    if (gpu->uinform_config.size > 0 && gpu->uinform_config.addr != 0)
    {
        jit_ctx.binding_buffers[0] = gpu->vram_ptr + gpu->uinform_config.addr;
    }
    for (int slot = 1; slot < MAX_BINDINGS; slot++)
    {
        if (gpu->texture_desc_addr[slot] != 0 && gpu->texture_desc_addr[slot] + sizeof(GpuTextureDescriptorVram) <= GPU_VRAM_SIZE)
        {
            GpuTextureDescriptorVram *vram_desc = (GpuTextureDescriptorVram *)(gpu->vram_ptr + gpu->texture_desc_addr[slot]);
            if (vram_desc->data_vram_addr != 0 && vram_desc->data_vram_addr < GPU_VRAM_SIZE)
            {
                gpu->textures[slot].data = (void *)(gpu->vram_ptr + vram_desc->data_vram_addr);
                gpu->textures[slot].width = vram_desc->width;
                gpu->textures[slot].height = vram_desc->height;
                gpu->textures[slot].channels = vram_desc->channels;
                gpu->textures[slot].filter = (FilterMode)vram_desc->filter;
                gpu->textures[slot].wrap = (WrapMode)vram_desc->wrap;
                jit_ctx.binding_buffers[slot] = &gpu->textures[slot];
            }
            else
            {
                jit_ctx.binding_buffers[slot] = NULL;
            }
        }
        else
        {
            jit_ctx.binding_buffers[slot] = NULL;
        }
    }
    
    jit_ctx.location_in_buffers[0] = fs_in_color;
    jit_ctx.location_in_buffers[1] = fs_in_uv;
    jit_ctx.location_out_buffers[0] = &out_color;
    gpu->fs_shader_func(&jit_ctx, NULL, fs_input, NULL);

    for (int lane = 0; lane < 16; lane++) 
    {
        if (!(shade_mask & (1 << lane))) continue;

        int px = x + (lane % 4);
        int py = y + (lane / 4);

        uint8_t r = color_to_u8(out_color.elem[0][lane]);
        uint8_t g = color_to_u8(out_color.elem[1][lane]);
        uint8_t b = color_to_u8(out_color.elem[2][lane]);
        uint8_t a = color_to_u8(fs_in_color->elem[3][lane]);
        put_pixel(gpu, px, py, RGBA_TO_UINT(r, g, b, a));
    }
}

void draw_triangle_simt_band(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, float u[3], float v[3], GpuState *gpu, int band_min_y, int band_max_y, RenderThreadArgs *args)
{
    TriangleContext ctx = {0};
    float inv_area;

    if (!setup_geometry_and_bounds(v0, v1, v2, gpu, band_min_y, band_max_y, &ctx, &inv_area)) 
    {
        return; 
    }

    setup_invariants(color, u, v, inv_area, &ctx);
    BuiltinFragmentInput fs_in = {0};

    for (int sy = ctx.stamp_min_y; sy <= ctx.stamp_max_y; sy += 4) 
    {
        float row_w0 = ctx.start_w0 + ((float)(sy - ctx.stamp_min_y)) * ctx.d_w0_dy;
        float row_w1 = ctx.start_w1 + ((float)(sy - ctx.stamp_min_y)) * ctx.d_w1_dy;
        float row_w2 = ctx.start_w2 + ((float)(sy - ctx.stamp_min_y)) * ctx.d_w2_dy;
        float row_w3 = ctx.start_w3 + ((float)(sy - ctx.stamp_min_y)) * ctx.d_w3_dy;

        for (int sx = ctx.stamp_min_x; sx <= ctx.stamp_max_x; sx += 4) 
        {
            float stamp_w0 = row_w0 + ((float)(sx - ctx.stamp_min_x)) * ctx.d_w0_dx;
            float stamp_w1 = row_w1 + ((float)(sx - ctx.stamp_min_x)) * ctx.d_w1_dx;
            float stamp_w2 = row_w2 + ((float)(sx - ctx.stamp_min_x)) * ctx.d_w2_dx;
            float stamp_w3 = row_w3 + ((float)(sx - ctx.stamp_min_x)) * ctx.d_w3_dx;

            float lane_w0[16], lane_w1[16], lane_w2[16], lane_w3[16];
            
            uint16_t exec_mask = evaluate_stamp_coverage(sx, sy, stamp_w0, stamp_w1, stamp_w2, stamp_w3, &ctx, lane_w0, lane_w1, lane_w2, lane_w3);

            if (exec_mask == 0x0000) continue;

            args->raster_exec_mask = exec_mask;

            SimtVec4 fs_in_color = {0};
            SimtVec2 fs_in_uv    = {0};
            uint16_t shade_mask = process_z_and_interpolate(sx, sy, exec_mask, lane_w0, lane_w1, lane_w2, lane_w3, &ctx, gpu, &fs_in_color, &fs_in_uv);
            if (shade_mask == 0x0000) continue;
            
            for (int lane = 0; lane < 16; lane++)
            {
                int dx = lane % 4;
                int dy = lane / 4;

                fs_in.gl_FragCoord.elem[0][lane] = (float)(sx + dx) + 0.5f;
                fs_in.gl_FragCoord.elem[1][lane] = (float)(sy + dy) + 0.5f;
            }
            execute_shader_and_write(sx, sy, shade_mask, gpu, &fs_in_color, &fs_in_uv, &fs_in);
        }
    }
}

void worker_rasterize_bands_simt_impl(RenderThreadArgs *args) 
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;

    TransformedVertex *vertices = args->transformed_vertices;
    Triangle *indices = args->assembled_triangles ? args->assembled_triangles : TRIANGLES_TABLE(gpu);
    uint32_t triangle_size = args->assembled_triangles ? args->assembled_triangle_count : args->triangle_size;

    int band_min_y = args->start_y;
    int band_max_y = args->end_y - 1;

    for (uint32_t i = 0; i < triangle_size; i++) 
    {
        Vec4 v0 = vertices[indices[i].a].pos;
        Vec4 v1 = vertices[indices[i].b].pos;
        Vec4 v2 = vertices[indices[i].c].pos;

        Col3 color = {
            .a_col = vertices[indices[i].a].color,
            .b_col = vertices[indices[i].b].color,
            .c_col = vertices[indices[i].c].color,
        };

        float u[3] = { vertices[indices[i].a].u, vertices[indices[i].b].u, vertices[indices[i].c].u };
        float v[3] = { vertices[indices[i].a].v, vertices[indices[i].b].v, vertices[indices[i].c].v };

        draw_triangle_simt_band(v0, v1, v2, color, u, v, gpu, band_min_y, band_max_y, args);
    }
}

void worker_rasterize_points_simt_impl(RenderThreadArgs *args)
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;

    TransformedVertex *vertices = args->transformed_vertices;
    uint32_t vertex_count = gpu->vbo_config.size;

    int band_min_y = args->start_y;
    int band_max_y = args->end_y - 1;

    float point_size = args->point_size > 0.0f ? args->point_size : 1.0f;
    float R = point_size * 0.5f;
    float R2 = R * R;

    uint32_t width = gpu->width;
    uint32_t height = gpu->height;

    for (uint32_t i = 0; i < vertex_count; i++)
    {
        Vec4 v = vertices[i].pos;
        float w = (v.w < 0.1f) ? 0.1f : v.w;
        float inv_w = 1.0f / w;

        float ndc_x = v.x * inv_w;
        float ndc_y = v.y * inv_w;

        float xc = (ndc_x + 1.0f) * 0.5f * (float)width;
        float yc = (1.0f - (ndc_y + 1.0f) * 0.5f) * (float)height;
        float z_depth = v.z * inv_w;

        int min_x = fmax(0, floorf(xc - R));
        int max_x = fmin((int)width - 1, ceilf(xc + R));
        int min_y = fmax(band_min_y, floorf(yc - R));
        int max_y = fmin(band_max_y, ceilf(yc + R));

        if (min_y > max_y || min_x > max_x) continue;

        int stamp_min_x = min_x & ~3;
        int stamp_max_x = (max_x + 4) & ~3;
        int stamp_min_y = min_y & ~3;
        int stamp_max_y = (max_y + 4) & ~3;

        BuiltinFragmentInput fs_in = {0};

        for (int sy = stamp_min_y; sy <= stamp_max_y; sy += 4)
        {
            for (int sx = stamp_min_x; sx <= stamp_max_x; sx += 4)
            {
                uint16_t exec_mask = 0;
                uint16_t shade_mask = 0;

                SimtVec4 fs_in_color = {0};
                SimtVec2 fs_in_uv = {0};

                const float inv_255 = 1.0f / 255.0f;
                float r_col = GET_R(vertices[i].color) * inv_255;
                float g_col = GET_G(vertices[i].color) * inv_255;
                float b_col = GET_B(vertices[i].color) * inv_255;
                float a_col = GET_A(vertices[i].color) * inv_255;

                for (int lane = 0; lane < 16; lane++)
                {
                    int lx = lane % 4;
                    int ly = lane / 4;
                    int px = sx + lx;
                    int py = sy + ly;

                    if (px < min_x || px > max_x || py < min_y || py > max_y) continue;

                    float pxf = (float)px + 0.5f;
                    float pyf = (float)py + 0.5f;

                    float dx = pxf - xc;
                    float dy = pyf - yc;

                    if (dx * dx + dy * dy <= R2)
                    {
                        exec_mask |= (1 << lane);

                        int idx = py * width + px;
                        if (z_depth < Z_BUFFER(gpu)[idx])
                        {
                            if (gpu->depth_write_enable == 1)
                            {
                                Z_BUFFER(gpu)[idx] = z_depth;
                            }
                            shade_mask |= (1 << lane);

                            fs_in_color.elem[0][lane] = r_col;
                            fs_in_color.elem[1][lane] = g_col;
                            fs_in_color.elem[2][lane] = b_col;
                            fs_in_color.elem[3][lane] = a_col;

                            float u_val = (R > 0.0f) ? (dx + R) / (2.0f * R) : 0.5f;
                            float v_val = (R > 0.0f) ? (dy + R) / (2.0f * R) : 0.5f;
                            fs_in_uv.elem[0][lane] = fmaxf(0.0f, fminf(1.0f, u_val));
                            fs_in_uv.elem[1][lane] = fmaxf(0.0f, fminf(1.0f, v_val));

                            fs_in.gl_FragCoord.elem[0][lane] = pxf;
                            fs_in.gl_FragCoord.elem[1][lane] = pyf;
                        }
                    }
                }

                if (shade_mask == 0) continue;
                args->raster_exec_mask = exec_mask;

                execute_shader_and_write(sx, sy, shade_mask, gpu, &fs_in_color, &fs_in_uv, &fs_in);
            }
        }
    }
}

void worker_rasterize_lines_simt_impl(RenderThreadArgs *args)
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;

    TransformedVertex *vertices = args->transformed_vertices;
    Edge *lines = (Edge *)args->assembled_triangles;
    uint32_t line_count = args->assembled_triangle_count;

    int band_min_y = args->start_y;
    int band_max_y = args->end_y - 1;
    float line_width = args->line_width > 0.0f ? args->line_width : 1.0f;
    float half_w = line_width * 0.5f;

    uint32_t width = gpu->width;
    uint32_t height = gpu->height;

    for (uint32_t i = 0; i < line_count; i++)
    {
        TransformedVertex p0 = vertices[lines[i].a];
        TransformedVertex p1 = vertices[lines[i].b];

        float w0 = (p0.pos.w < 0.1f) ? 0.1f : p0.pos.w;
        float w1 = (p1.pos.w < 0.1f) ? 0.1f : p1.pos.w;

        float inv_w0 = 1.0f / w0;
        float inv_w1 = 1.0f / w1;

        float x0 = (p0.pos.x * inv_w0 + 1.0f) * 0.5f * (float)width;
        float y0 = (1.0f - (p0.pos.y * inv_w0 + 1.0f) * 0.5f) * (float)height;

        float x1 = (p1.pos.x * inv_w1 + 1.0f) * 0.5f * (float)width;
        float y1 = (1.0f - (p1.pos.y * inv_w1 + 1.0f) * 0.5f) * (float)height;

        float dx = x1 - x0;
        float dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);

        float nx, ny;
        if (len < 1e-6f) 
        {
            nx = 0.0f;
            ny = half_w;
        } 
        else 
        {
            nx = -dy / len * half_w;
            ny =  dx / len * half_w;
        }

        float sx[4] = { x0 - nx, x0 + nx, x1 + nx, x1 - nx };
        float sy[4] = { y0 - ny, y0 + ny, y1 + ny, y1 - ny };

        TransformedVertex q[4];
        for (int k = 0; k < 4; k++) 
        {
            TransformedVertex ref = (k < 2) ? p0 : p1;
            q[k] = ref;

            float ndc_x = (sx[k] / (float)width) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (sy[k] / (float)height) * 2.0f;

            q[k].pos.x = ndc_x * ref.pos.w;
            q[k].pos.y = ndc_y * ref.pos.w;
        }

        Col3 c1 = { q[0].color, q[1].color, q[2].color };
        float u1[3] = { q[0].u, q[1].u, q[2].u };
        float v1[3] = { q[0].v, q[1].v, q[2].v };
        draw_triangle_simt_band(q[0].pos, q[1].pos, q[2].pos, c1, u1, v1, gpu, band_min_y, band_max_y, args);

        Col3 c2 = { q[0].color, q[2].color, q[3].color };
        float u2[3] = { q[0].u, q[2].u, q[3].u };
        float v2[3] = { q[0].v, q[2].v, q[3].v };
        draw_triangle_simt_band(q[0].pos, q[2].pos, q[3].pos, c2, u2, v2, gpu, band_min_y, band_max_y, args);
    }
}

void worker_compute_simt_impl(RenderThreadArgs *args)
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;

    uint32_t total_wg = gpu->dispatch_total_workgroups;
    if (total_wg == 0) return;

    // Find worker ID by matching args pointer
    //int worker_id = 0;
    // for (int i = 0; i < NUM_RENDER_THREADS; i++) {
    //     if (args == &args->orig_gpu->refresh_thread) {} // dummy comparison
    // }
    // Calculate chunk for this thread
    //uint32_t chunk = (total_wg + NUM_RENDER_THREADS - 1) / NUM_RENDER_THREADS;
    
    // Determine worker_id based on address offset from pool
    // Note: RenderThreadArgs array element index:
    uint32_t wg_start = 0;
    uint32_t wg_end = 0;
    // Each thread gets assigned a range of workgroups
    // To be thread-agnostic, args contains start_block and end_block set by dispatch
    wg_start = args->start_block;
    wg_end = args->end_block;

    uint32_t Sx = gpu->cs_local_size_x > 0 ? gpu->cs_local_size_x : 1;
    uint32_t Sy = gpu->cs_local_size_y > 0 ? gpu->cs_local_size_y : 1;
    uint32_t Sz = gpu->cs_local_size_z > 0 ? gpu->cs_local_size_z : 1;
    uint32_t local_count = Sx * Sy * Sz;
    uint32_t warps_per_wg = (local_count + SIMT_WIDTH - 1) / SIMT_WIDTH;

    uint32_t Gx = gpu->dispatch_group_count_x > 0 ? gpu->dispatch_group_count_x : 1;
    uint32_t Gy = gpu->dispatch_group_count_y > 0 ? gpu->dispatch_group_count_y : 1;
    uint32_t Gz = gpu->dispatch_group_count_z > 0 ? gpu->dispatch_group_count_z : 1;

    for (uint32_t wg_linear = wg_start; wg_linear < wg_end; wg_linear++)
    {
        uint32_t Wx = wg_linear % Gx;
        uint32_t Wy = (wg_linear / Gx) % Gy;
        uint32_t Wz = wg_linear / (Gx * Gy);

        uint8_t shared_mem[MAX_SHARED_MEM_SIZE];
        memset(shared_mem, 0, sizeof(shared_mem));

        uint32_t num_warps = warps_per_wg > 0 ? warps_per_wg : 1;
        uint8_t *warp_spill = (uint8_t *)calloc(num_warps, 2048);

        uint32_t num_phases = gpu->cs_barrier_count + 1;

        for (uint32_t phase = 0; phase < num_phases; phase++)
        {
            for (uint32_t w = 0; w < warps_per_wg; w++)
            {
                uint32_t base_lane = w * SIMT_WIDTH;
                BuiltinComputeInput cs_in = {0};

                for (int i = 0; i < SIMT_WIDTH; i++)
                {
                    uint32_t linear = base_lane + i;
                    uint32_t lx = linear % Sx;
                    uint32_t ly = (linear / Sx) % Sy;
                    uint32_t lz = linear / (Sx * Sy);

                    cs_in.gl_LocalInvocationID.elem[0][i] = (float)lx;
                    cs_in.gl_LocalInvocationID.elem[1][i] = (float)ly;
                    cs_in.gl_LocalInvocationID.elem[2][i] = (float)lz;

                    cs_in.gl_GlobalInvocationID.elem[0][i] = (float)(Wx * Sx + lx);
                    cs_in.gl_GlobalInvocationID.elem[1][i] = (float)(Wy * Sy + ly);
                    cs_in.gl_GlobalInvocationID.elem[2][i] = (float)(Wz * Sz + lz);

                    cs_in.gl_LocalInvocationIndex[i] = (float)linear;

                    cs_in.gl_WorkGroupID.elem[0][i] = (float)Wx;
                    cs_in.gl_WorkGroupID.elem[1][i] = (float)Wy;
                    cs_in.gl_WorkGroupID.elem[2][i] = (float)Wz;

                    cs_in.gl_NumWorkGroups.elem[0][i] = (float)Gx;
                    cs_in.gl_NumWorkGroups.elem[1][i] = (float)Gy;
                    cs_in.gl_NumWorkGroups.elem[2][i] = (float)Gz;

                    cs_in.gl_WorkGroupSize.elem[0][i] = (float)Sx;
                    cs_in.gl_WorkGroupSize.elem[1][i] = (float)Sy;
                    cs_in.gl_WorkGroupSize.elem[2][i] = (float)Sz;
                }

                ExecutionContext ectx = {0};
                ectx.shared_memory = shared_mem;
                ectx.spill_buffer = warp_spill ? (warp_spill + w * 2048) : NULL;
                ectx.current_phase = phase;

                // Bind UBO
                if (gpu->uinform_config.size > 0 && gpu->uinform_config.addr != 0) {
                    ectx.binding_buffers[0] = gpu->vram_ptr + gpu->uinform_config.addr;
                }

                // Bind SSBOs / Resources
                for (int slot = 0; slot < MAX_BINDINGS; slot++) {
                    if (gpu->ssbo_config[slot].addr != 0 && gpu->ssbo_config[slot].size > 0) {
                        ectx.binding_buffers[slot] = gpu->vram_ptr + gpu->ssbo_config[slot].addr;
                    }
                }

                if (gpu->cs_shader_func) {
                    gpu->cs_shader_func(&ectx, NULL, NULL, &cs_in);
                }
            }
        }
        if (warp_spill) {
            free(warp_spill);
        }
    }
}
