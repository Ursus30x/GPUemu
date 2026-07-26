
#include "renderer.h"
#include "math3d.h"
#include "math.h"
#include "debug_gpu.h"
#include "qemu/thread.h"
#include "asm.h"
#define PRINT_V4(v) DEBUG_PRINT("[%f, %f, %f, %f]\n", v.x, v.y, v.z, v.w);

static RendererThreads render;

void put_pixel(GpuState *gpu, int x, int y, uint32_t color)
{
    // if(gpu->use_legacy_asm) 
    // {
    //     gpu->pRegs[REG_PX].f32 = (float)x;
    //     gpu->pRegs[REG_PY].f32 = (float)y;
    //     uint8_t r  = (color >> 16) & 0xFF;
    //     uint8_t g  = (color >> 8) & 0xFF;
    //     uint8_t b  = color & 0xFF;
    //     gpu->pRegs[REG_PR].u32 = r;
    //     gpu->pRegs[REG_PG].u32 = g;
    //     gpu->pRegs[REG_PB].u32 = b;
    //     exec_shader(gpu, gpu->fs_code_addr);
    //     color = (gpu->pRegs[REG_PR].u32 << 16) |
    //             (gpu->pRegs[REG_PG].u32 << 8)  |
    //                 gpu->pRegs[REG_PB].u32;
    // }

    FB(gpu)[y * GPU_FB_WIDTH + x] = color;
}

void draw_line(GpuState *gpu, int x0, int y0, int x1, int y1, uint32_t color1, uint32_t color2) 
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int length = (int)sqrtf((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0));
    if (length == 0) length = 1;
    int step = 0;

    for (;;)
    {
        float t = (float)step / length;

        uint8_t a1 = (color1 >> 24) & 0xFF;
        uint8_t r1 = (color1 >> 16) & 0xFF;
        uint8_t g1 = (color1 >> 8) & 0xFF;
        uint8_t b1 = color1 & 0xFF;

        uint8_t a2 = (color2 >> 24) & 0xFF;
        uint8_t r2 = (color2 >> 16) & 0xFF;
        uint8_t g2 = (color2 >> 8) & 0xFF;
        uint8_t b2 = color2 & 0xFF;

        uint32_t color = ((uint32_t)(a1 + t*(a2 - a1)) << 24) |
                         ((uint32_t)(r1 + t*(r2 - r1)) << 16) |
                         ((uint32_t)(g1 + t*(g2 - g1)) << 8) |
                         ((uint32_t)(b1 + t*(b2 - b1)));

        put_pixel(gpu, x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        step++;
    }
}


float edge_func(Vec3 a, Vec3 b, Vec3 c)
{
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}
static inline float project_depth(Vec4 v)
{
    float w = (v.w < 0.1f) ? 0.1f : v.w;
    return v.z / w;
}

static void draw_triangle_band(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu, int band_min_y, int band_max_y)
{
    uint32_t width = gpu->width;
    uint32_t height =  gpu->height;
    Vec3 s[3];
    Vec4 v[3] = {v0, v1, v2};

    for(int i = 0; i < 3; i++)
    {
        float w = (v[i].w < 0.1f) ? 0.1f : v[i].w;
        float inv_w = 1.0f / w;

        float ndc_x = v[i].x * inv_w;
        float ndc_y = v[i].y * inv_w;

        s[i].x = (ndc_x + 1.0f) * 0.5f * width;
        s[i].y = (1.0f - (ndc_y + 1.0f) * 0.5f) * height;
        s[i].z = inv_w;
    }

    float area = edge_func(s[0], s[1], s[2]);

    int min_x = fmax(0, fmin(s[0].x, fmin(s[1].x, s[2].x)));
    int max_x = fmin(width-1, fmax(s[0].x, fmax(s[1].x, s[2].x)));
    
    // Intersect the triangle bound's vertical footprint with the thread's assigned scanline band
    int min_y = fmax(band_min_y, fmin(s[0].y, fmin(s[1].y, s[2].y)));
    int max_y = fmin(band_max_y, fmax(s[0].y, fmax(s[1].y, s[2].y)));

    if (min_y > max_y || min_x > max_x) return;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            Vec3 p = {x + 0.5f, y + 0.5f, 0};
            float w0 = edge_func(s[1], s[2], p) / area;
            float w1 = edge_func(s[2], s[0], p) / area;
            float w2 = edge_func(s[0], s[1], p) / area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float z = w0 * (1.0f/s[0].z) + w1 * (1.0f/s[1].z) + w2 * (1.0f/s[2].z);
                int idx = y * width + x;

                if (z < Z_BUFFER(gpu)[idx]) 
                {
                    Z_BUFFER(gpu)[idx] = z;
                    uint8_t r = (uint8_t)(w0 * GET_R(color.a_col) + w1 * GET_R(color.b_col) + w2 * GET_R(color.c_col));
                    uint8_t g = (uint8_t)(w0 * GET_G(color.a_col) + w1 * GET_G(color.b_col) + w2 * GET_G(color.c_col));
                    uint8_t b = (uint8_t)(w0 * GET_B(color.a_col) + w1 * GET_B(color.b_col) + w2 * GET_B(color.c_col));
                    put_pixel(gpu, x, y, RGB_TO_UINT(r,g,b));
                }
            }
        }
    }
}


void draw_triangle(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu)
{
    draw_triangle_band(v0, v1, v2, color, gpu, 0, gpu->height - 1);
}

// pass 1: Parallel Vertex Shader execution across all VBO elements
static void worker_transform_vertices_impl(RenderThreadArgs *args) {
    GpuState local_gpu = *(args->orig_gpu); 
    GpuState *gpu = &local_gpu;

    Vec3 *vertices = VERTEX_TABLE(gpu);
    TransformedVertex *out_vertices = args->transformed_vertices;

    for (uint32_t i = args->start_idx; i < args->end_idx; i++)
    {
        Vec4 v = {vertices[i].x, vertices[i].y, vertices[i].z, 1.0f};
        gpu->v_pos.right = v;

        uint32_t color = vertices[i].rgba;
        gpu->pRegs[REG_PR].u32 = GET_R(color);
        gpu->pRegs[REG_PG].u32 = GET_G(color);
        gpu->pRegs[REG_PB].u32 = GET_B(color);

        exec_shader(gpu, gpu->vs_code_addr);

        out_vertices[i].pos = gpu->v_out.right;
        out_vertices[i].color = RGB_TO_UINT(
            (uint8_t)gpu->pRegs[REG_PR].u32,
            (uint8_t)gpu->pRegs[REG_PG].u32,
            (uint8_t)gpu->pRegs[REG_PB].u32);
    }
    
   
}

static void bind_ubo_to_context(GpuState *gpu, ExecutionContext *ectx)
{
    if (gpu->uinform_config.size > 0 && gpu->uinform_config.addr != 0)
    {
        void *ubo_data = gpu->vram_ptr + gpu->uinform_config.addr;
        ectx->binding_buffers[0] = ubo_data;
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
        if (base_idx + SIMT_WIDTH > total_vertices) {
            uint32_t active_lanes = total_vertices - base_idx;
            exec_mask = (1 << active_lanes) - 1;
        }

        SimtVec3 in_vec;
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            if (exec_mask & (1 << lane))
            {
                uint32_t i = base_idx + lane;
                in_vec.elem[0][lane] = vertices[i].x;
                in_vec.elem[1][lane] = vertices[i].y;
                in_vec.elem[2][lane] = vertices[i].z;
            }
            else
            {
                in_vec.elem[0][lane] = 0.0f;
                in_vec.elem[1][lane] = 0.0f;
                in_vec.elem[2][lane] = 0.0f;
            }
        }

        ExecutionContext jit_ctx = {0};
        BuiltinVertexOutput vs_out = {0};
        bind_ubo_to_context(gpu, &jit_ctx);
        jit_ctx.location_in_buffers[0] = &in_vec;
        gpu->vs_shader_func(&jit_ctx, &vs_out, NULL);
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
            }
        }
    }
}

static void worker_rasterize_bands_impl(RenderThreadArgs *args) 
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;

    TransformedVertex *vertices = args->transformed_vertices;
    Triangle *indices = TRIANGLES_TABLE(gpu);
    uint32_t triangle_size = args->triangle_size;

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

        draw_triangle_band(v0, v1, v2, color, gpu, band_min_y, band_max_y);
   }
}

// =================================================================
// PHASE 1: Geometry, Viewport, and Bounds
// =================================================================
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
        ctx->inv_z[i] = 1.0f / inv_w;

        float ndc_x = v[i].x * inv_w;
        float ndc_y = v[i].y * inv_w;

        ctx->s[i].x = (ndc_x + 1.0f) * 0.5f * width;
        ctx->s[i].y = (1.0f - (ndc_y + 1.0f) * 0.5f) * height;
        ctx->s[i].z = inv_w; 
    }

    float area = edge_func(ctx->s[0], ctx->s[1], ctx->s[2]);
    //if (area == 0.0f) return false; 
    
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

// =================================================================
// PHASE 2: Pineda Gradients & Color Invariants
// =================================================================
static void setup_invariants(Col3 color, float inv_area, TriangleContext *ctx) 
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

    ctx->r_inv_w[0] = GET_R(color.a_col) * ctx->s_inv_w[0];
    ctx->g_inv_w[0] = GET_G(color.a_col) * ctx->s_inv_w[0];
    ctx->b_inv_w[0] = GET_B(color.a_col) * ctx->s_inv_w[0];

    ctx->r_inv_w[1] = GET_R(color.b_col) * ctx->s_inv_w[1];
    ctx->g_inv_w[1] = GET_G(color.b_col) * ctx->s_inv_w[1];
    ctx->b_inv_w[1] = GET_B(color.b_col) * ctx->s_inv_w[1];

    ctx->r_inv_w[2] = GET_R(color.c_col) * ctx->s_inv_w[2];
    ctx->g_inv_w[2] = GET_G(color.c_col) * ctx->s_inv_w[2];
    ctx->b_inv_w[2] = GET_B(color.c_col) * ctx->s_inv_w[2];

    Vec3 start_p = {ctx->stamp_min_x + 0.5f, ctx->stamp_min_y + 0.5f, 0};
    ctx->start_w0 = edge_func(ctx->s[1], ctx->s[2], start_p) * inv_area;
    ctx->start_w1 = edge_func(ctx->s[2], ctx->s[0], start_p) * inv_area;
    ctx->start_w2 = edge_func(ctx->s[0], ctx->s[1], start_p) * inv_area;
}

// =================================================================
// PHASE 3: Stamp Coverage Evaluation (Step A)
// =================================================================
static uint16_t evaluate_stamp_coverage(int x, int y, float stamp_w0, float stamp_w1, float stamp_w2, TriangleContext *ctx, float *lane_w0, float *lane_w1, float *lane_w2) 
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

        lane_w0[lane] = w0;
        lane_w1[lane] = w1;
        lane_w2[lane] = w2;

        bool in_bounds = (px >= ctx->min_x && px <= ctx->max_x && py >= ctx->min_y && py <= ctx->max_y);
        bool in_tri = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
        
        if (in_bounds && in_tri) 
        {
            exec_mask |= (1 << lane);
        }
    }
    return exec_mask;
}

// =================================================================
// PHASE 4: Early-Z and Varying Interpolation 
// =================================================================
static uint16_t process_z_and_interpolate(int x, int y, uint16_t exec_mask, float *lane_w0, float *lane_w1, float *lane_w2, TriangleContext *ctx, GpuState *gpu, SimtVec3 *fs_in_color) 
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
            Z_BUFFER(gpu)[idx] = z;
            shade_mask |= (1 << lane);

            float pixel_inv_w = w0 * ctx->s_inv_w[0] + w1 * ctx->s_inv_w[1] + w2 * ctx->s_inv_w[2];
            float pixel_w = 1.0f / pixel_inv_w;

            fs_in_color->elem[0][lane] = (w0 * ctx->r_inv_w[0] + w1 * ctx->r_inv_w[1] + w2 * ctx->r_inv_w[2]) * pixel_w;
            fs_in_color->elem[1][lane] = (w0 * ctx->g_inv_w[0] + w1 * ctx->g_inv_w[1] + w2 * ctx->g_inv_w[2]) * pixel_w;
            fs_in_color->elem[2][lane] = (w0 * ctx->b_inv_w[0] + w1 * ctx->b_inv_w[1] + w2 * ctx->b_inv_w[2]) * pixel_w;
        }
    }
    return shade_mask;
}

// =================================================================
// PHASE 5: Shading and Output (Step C & D)
// =================================================================
static void execute_shader_and_write(int x, int y, uint16_t shade_mask, GpuState *gpu, SimtVec3 *fs_in_color, BuiltinFragmentInput* fs_input) 
{
    SimtVec3 out_color = {0};
    ExecutionContext jit_ctx = {0};
    
    bind_ubo_to_context(gpu, &jit_ctx);
    
    jit_ctx.location_in_buffers[0] = fs_in_color;
    jit_ctx.location_out_buffers[0] = &out_color;
    gpu->fs_shader_func(&jit_ctx, NULL, fs_input);

    for (int lane = 0; lane < 16; lane++) 
    {
        if (!(shade_mask & (1 << lane))) continue;

        int px = x + (lane % 4);
        int py = y + (lane / 4);

        uint8_t r = (uint8_t)out_color.elem[0][lane];
        uint8_t g = (uint8_t)out_color.elem[1][lane];
        uint8_t b = (uint8_t)out_color.elem[2][lane];
        put_pixel(gpu, px, py, RGB_TO_UINT(r, g, b));
    }
}


static void draw_triangle_simt_band(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu, int band_min_y, int band_max_y, RenderThreadArgs *args)
{
    TriangleContext ctx = {0};
    float inv_area;

    if (!setup_geometry_and_bounds(v0, v1, v2, gpu, band_min_y, band_max_y, &ctx, &inv_area)) 
    {
        return; 
    }

    setup_invariants(color, inv_area, &ctx);
    BuiltinFragmentInput fs_in = {0};
    for (int y = ctx.stamp_min_y; y < ctx.stamp_max_y; y += 4) 
    {
        float row_w0 = ctx.start_w0 + (y - ctx.stamp_min_y) * ctx.d_w0_dy;
        float row_w1 = ctx.start_w1 + (y - ctx.stamp_min_y) * ctx.d_w1_dy;
        float row_w2 = ctx.start_w2 + (y - ctx.stamp_min_y) * ctx.d_w2_dy;

        for (int x = ctx.stamp_min_x; x < ctx.stamp_max_x; x += 4) 
        {
            float stamp_w0 = row_w0 + (x - ctx.stamp_min_x) * ctx.d_w0_dx;
            float stamp_w1 = row_w1 + (x - ctx.stamp_min_x) * ctx.d_w1_dx;
            float stamp_w2 = row_w2 + (x - ctx.stamp_min_x) * ctx.d_w2_dx;

            float lane_w0[16], lane_w1[16], lane_w2[16];
            
            uint16_t exec_mask = evaluate_stamp_coverage(x, y, stamp_w0, stamp_w1, stamp_w2, &ctx, lane_w0, lane_w1, lane_w2);

            if (exec_mask == 0x0000) continue;

            args->raster_exec_mask = exec_mask;

            SimtVec3 fs_in_color = {0};
            uint16_t shade_mask = process_z_and_interpolate(x, y, exec_mask, lane_w0, lane_w1, lane_w2, &ctx, gpu, &fs_in_color);
            if (shade_mask == 0x0000) continue;
            for (int lane = 0; lane < 16; lane++)
            {
                int dx = lane % 4;
                int dy = lane / 4;

                // 1. Screen Space X and Y (centered at +0.5f) 
                fs_in.gl_FragCoord.elem[0][lane] = (float)(x + dx) + 0.5f;
                fs_in.gl_FragCoord.elem[1][lane] = (float)(y + dy) + 0.5f;

                // // 2. Barycentric weights for this lane
                // float w0 = lane_w0[lane];
                // float w1 = lane_w1[lane];
                // float w2 = lane_w2[lane];

                // // 3. Interpolated Z (Depth) and W (1/W perspective term)
                // fs_in.gl_FragCoord.z[lane] = w0 * ctx.v0_z     + w1 * ctx.v1_z     + w2 * ctx.v2_z;
                // fs_in.gl_FragCoord.w[lane] = w0 * ctx.v0_inv_w + w1 * ctx.v1_inv_w + w2 * ctx.v2_inv_w;
            }
            execute_shader_and_write(x, y, shade_mask, gpu, &fs_in_color, &fs_in);
        }
    }
}
static void worker_rasterize_bands_simt_impl(RenderThreadArgs *args) 
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;

    TransformedVertex *vertices = args->transformed_vertices;
    Triangle *indices = TRIANGLES_TABLE(gpu);
    uint32_t triangle_size = args->triangle_size;

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
        draw_triangle_simt_band(v0, v1, v2, color, gpu, band_min_y, band_max_y, args);
    }
}
static void worker_wireframe_vertices_impl(RenderThreadArgs *args) 
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;
    
    uint32_t width = gpu->width;
    uint32_t height = gpu->height;
    Vec3 *vertices = VERTEX_TABLE(gpu);

    for(uint32_t i = args->start_idx; i < args->end_idx; i++)
    {
        Vec4 v = {vertices[i].x, vertices[i].y, vertices[i].z, 1.0f};
        gpu->v_pos.right = v;

        uint32_t color = vertices[i].rgba;
        uint8_t r  = GET_R(color);
        uint8_t g  = GET_G(color);
        uint8_t b  = GET_B(color);
        gpu->pRegs[REG_PR].u32 = r;
        gpu->pRegs[REG_PG].u32 = g;
        gpu->pRegs[REG_PB].u32 = b;
        
        exec_shader(gpu, gpu->vs_code_addr);

        vertices[i].rgba = RGB_TO_UINT(
            (uint8_t)gpu->pRegs[REG_PR].u32,
            (uint8_t)gpu->pRegs[REG_PG].u32,
            (uint8_t)gpu->pRegs[REG_PB].u32);

        Vec4 tv = gpu->v_out.right;
        float ndc_x = tv.x / tv.w;
        float ndc_y = tv.y / tv.w;
        args->px[i] = (int)((ndc_x*0.5f + 0.5f) * width);
        args->py[i] = (int)((-ndc_y*0.5f + 0.5f) * height);
    }
}

static void worker_wireframe_edges_impl(RenderThreadArgs *args) 
{
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;
    
    Edge *edges = EDGES_TABLE(gpu);
    Vec3 *vertices = VERTEX_TABLE(gpu);

    for(uint32_t i = args->start_idx; i < args->end_idx; i++)
    {
        Edge e = edges[i];
        DEBUG_PRINT("[GPU State] px[e.a]: %u, py[e.a]: %u, px[e.b]: %u, py[e.b]: %u, e.a: %u, e.b %u\n",
            args->px[e.a], args->py[e.a], args->px[e.b], args->py[e.b], e.a, e.b);
        draw_line(gpu, args->px[e.a], args->py[e.a], args->px[e.b], args->py[e.b], vertices[e.a].rgba, vertices[e.b].rgba);
    }
}

QemuMutex jit_mutex;
static void* worker_thread(void* arg) 
{
    RenderThreadArgs* my_args = (RenderThreadArgs*)arg;
    int local_generation = 0;

    while (1) 
    {
        qemu_mutex_lock(&render.pool_mutex);
        while (local_generation == render.work_generation)
        {
            qemu_cond_wait(&render.pool_cond, &render.pool_mutex);
        }
        local_generation = render.work_generation;
        RenderTaskType task_to_run = render.current_task;
        qemu_mutex_unlock(&render.pool_mutex);

        if (task_to_run == TASK_EXIT) break;

        switch (task_to_run)
        {
            case TASK_TRANSFORM_VERTICES:      worker_transform_vertices_impl(my_args); break;
            case TASK_TRANSFORM_VERTICES_SIMT: worker_transform_vertices_simt_impl(my_args); break;
            case TASK_RASTERIZE_BANDS:         worker_rasterize_bands_impl(my_args); break;
            case TASK_RASTERIZE_BANDS_SIMT:    worker_rasterize_bands_simt_impl(my_args); break;
            case TASK_WIREFRAME_VERTICES:      worker_wireframe_vertices_impl(my_args); break;
            case TASK_WIREFRAME_EDGES:         worker_wireframe_edges_impl(my_args); break;
            default: break;
        }

        qemu_mutex_lock(&render.pool_mutex);
        render.completed_workers++;
        if (render.completed_workers == NUM_RENDER_THREADS) 
        {
            qemu_cond_signal(&render.main_cond);
        }
        qemu_mutex_unlock(&render.pool_mutex);
    }
    return NULL;
}

void init_thread_pool(void) 
{
    render.work_generation = 0;
    render.completed_workers = 0;
    render.current_task = TASK_NONE;
    render.threads_initialized = 0;
    if (render.threads_initialized) return;

    qemu_mutex_init(&render.pool_mutex);
    qemu_cond_init(&render.pool_cond);
    qemu_cond_init(&render.main_cond);
    qemu_mutex_init(&jit_mutex);

    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        qemu_thread_create(&render.threads[i], "gpu_render_worker", worker_thread, &render.args[i], QEMU_THREAD_JOINABLE);
    }
    render.threads_initialized = 1;
}

static void dispatch_task(RenderTaskType task) 
{
    qemu_mutex_lock(&render.pool_mutex);
    render.current_task = task;
    render.completed_workers = 0;
    render.work_generation++;
    qemu_cond_broadcast(&render.pool_cond);

    while (render.completed_workers < NUM_RENDER_THREADS)
    {
        qemu_cond_wait(&render.main_cond, &render.pool_mutex);
    }
    qemu_mutex_unlock(&render.pool_mutex);
}

void gpu_render_triangles_simt(void *opaque)
{
    
    GpuState *gpu = opaque;
    if (gpu->gpu_mode == GPU_MODE_IDLE)
    {
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;
    
    uint32_t vertex_size = gpu->vbo_config.size;
    uint32_t triangle_size = gpu->edge_config.size; 

    TransformedVertex *transformed_vertices = malloc(sizeof(TransformedVertex) * vertex_size);
    
    if (!transformed_vertices)
    {
        gpu->gpu_mode = GPU_MODE_3D;
        return;
    }

    // PASS 1: Vertex Transformations (SIMT 16-element blocks)
    uint32_t total_vertex_blocks = (vertex_size + 15) / SIMT_WIDTH;
    uint32_t chunk_v_blocks = total_vertex_blocks / NUM_RENDER_THREADS;
    
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        render.args[i].orig_gpu = gpu;
        render.args[i].transformed_vertices = transformed_vertices;
        
        // Distribute work by 16-lane blocks rather than single indexes
        render.args[i].start_block = i * chunk_v_blocks;
        render.args[i].end_block = (i == NUM_RENDER_THREADS - 1) ? total_vertex_blocks : (i + 1) * chunk_v_blocks;
    }
    dispatch_task(TASK_TRANSFORM_VERTICES_SIMT);

    //PASS 2: Rasterization (SIMT 4x4 Pixel Stamps / 16 Lanes)
    uint32_t height = gpu->height;
    uint32_t chunk_y = height / NUM_RENDER_THREADS;
    chunk_y = (chunk_y + 3) & ~3; 

    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        render.args[i].orig_gpu = gpu;
        render.args[i].transformed_vertices = transformed_vertices;
        render.args[i].triangle_size = triangle_size;
        
        render.args[i].start_y = i * chunk_y;
        render.args[i].jit_ctx_fs = gpu->jit_ctx_fs;
        uint32_t end_y = (i == NUM_RENDER_THREADS - 1) ? height : (i + 1) * chunk_y;
        if (end_y > height) 
        {
            end_y = height;
        }
        render.args[i].end_y = end_y;
    }
    dispatch_task(TASK_RASTERIZE_BANDS_SIMT);

    free(transformed_vertices);
    gpu->gpu_mode = GPU_MODE_3D;
}

void gpu_render_triangles(void *opaque)
{
      GpuState *gpu = opaque;
    if(gpu->gpu_mode == GPU_MODE_IDLE)
    {
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;
    
    uint32_t vertex_size = gpu->vbo_config.size;
    uint32_t triangle_size = gpu->edge_config.size; 

    // Allocate geometry cache for vertex pre-shading pass
    TransformedVertex *transformed_vertices = malloc(sizeof(TransformedVertex) * vertex_size);
    if (!transformed_vertices)
    {
        gpu->gpu_mode = GPU_MODE_3D;
        return;
    }

    // pass 1: Run vertex transformations in parallel
    uint32_t chunk_v = vertex_size / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        render.args[i].orig_gpu = gpu;
        render.args[i].transformed_vertices = transformed_vertices;
        render.args[i].start_idx = i * chunk_v;
        render.args[i].end_idx = (i == NUM_RENDER_THREADS - 1) ? vertex_size : (i + 1) * chunk_v;
    }
    dispatch_task(TASK_TRANSFORM_VERTICES);

    // pass 2: Rasterize horizontal screen scanline bands in parallel
    uint32_t height = gpu->height;
    uint32_t chunk_y = height / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        render.args[i].orig_gpu = gpu;
        render.args[i].transformed_vertices = transformed_vertices;
        render.args[i].triangle_size = triangle_size;
        render.args[i].start_y = i * chunk_y;
        render.args[i].end_y = (i == NUM_RENDER_THREADS - 1) ? height : (i + 1) * chunk_y;
    }
    dispatch_task(TASK_RASTERIZE_BANDS);

    free(transformed_vertices);
    gpu->gpu_mode = GPU_MODE_3D;
}

void gpu_render_wireframe(void *opaque)
{
    GpuState *gpu = opaque;
    if(gpu->gpu_mode == GPU_MODE_IDLE)
    {
        DEBUG_PRINT("[Render Frame] GPU IS IDLE\n");
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;


    debug_dump_edges(opaque);
    debug_dump_vertices(opaque);
    debug_dump_ubo(opaque);

    uint32_t vertex_size = gpu->vbo_config.size;
    uint32_t edges_size =  gpu->edge_config.size;
    
    DEBUG_PRINT("[GPU State] Width: %u, Height: %u, Vertex Size: %u, Edge Size: %u\n",
       gpu->width, gpu->height, gpu->vbo_config.size, gpu->edge_config.size);

    uint32_t *px = malloc(sizeof(uint32_t) * vertex_size);
    uint32_t *py = malloc(sizeof(uint32_t) * vertex_size);

    // pass 1: Run vertex transformations in parallel
    uint32_t chunk_v = vertex_size / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        render.args[i].orig_gpu = gpu;
        render.args[i].px = px;
        render.args[i].py = py;
        render.args[i].start_idx = i * chunk_v;
        render.args[i].end_idx = (i == NUM_RENDER_THREADS - 1) ? vertex_size : (i + 1) * chunk_v;
    }
    dispatch_task(TASK_WIREFRAME_VERTICES);

    DEBUG_PRINT("[Render Frame] Drawing lines\n");

    // pass 2: Rasterize edges chunks in parallel
    uint32_t chunk_e = edges_size / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++)
    {
        render.args[i].start_idx = i * chunk_e;
        render.args[i].end_idx = (i == NUM_RENDER_THREADS - 1) ? edges_size : (i + 1) * chunk_e;
    }
    dispatch_task(TASK_WIREFRAME_EDGES);

    free(px);
    free(py);
    gpu->gpu_mode = GPU_MODE_3D;
}