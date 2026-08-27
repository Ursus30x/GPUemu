#include "renderer.h"
#include "primitive_assembly.h"
#include "rasterizer_simt.h"
#include "math3d.h"
#include <math.h>
#include "debug_gpu.h"
#include "qemu/thread.h"
#include "asm.h"

#define PRINT_V4(v) DEBUG_PRINT("[%f, %f, %f, %f]\n", v.x, v.y, v.z, v.w);

static RendererThreads render;
QemuMutex jit_mutex;

static inline float get_blend_factor(GpuBlendFactor factor, float src_a, float dst_a, float src_c, float dst_c) 
{
    switch (factor) {
        case GPU_BLEND_FACTOR_ZERO:                return 0.0f;
        case GPU_BLEND_FACTOR_ONE:                 return 1.0f;
        case GPU_BLEND_FACTOR_SRC_ALPHA:           return src_a;
        case GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return 1.0f - src_a;
        case GPU_BLEND_FACTOR_DST_ALPHA:           return dst_a;
        case GPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return 1.0f - dst_a;
        case GPU_BLEND_FACTOR_SRC_COLOR:           return src_c;
        case GPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return 1.0f - src_c;
        case GPU_BLEND_FACTOR_DST_COLOR:           return dst_c;
        case GPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return 1.0f - dst_c;
        default:                                   return 1.0f;
    }
}

static inline uint8_t color_to_u8(float c)
{
    if (c <= 0.0f) return 0;
    float scaled = c * 255.0f;
    if (scaled >= 255.0f) return 255;
    return (uint8_t)(scaled + 0.5f);
}

void put_pixel(GpuState *gpu, int x, int y, uint32_t color)
{
    if(gpu->use_legacy_asm) 
    {
        gpu->pRegs[REG_PX].f32 = (float)x;
        gpu->pRegs[REG_PY].f32 = (float)y;
        uint8_t r  = (color >> 16) & 0xFF;
        uint8_t g  = (color >> 8) & 0xFF;
        uint8_t b  = color & 0xFF;
        gpu->pRegs[REG_PR].u32 = r;
        gpu->pRegs[REG_PG].u32 = g;
        gpu->pRegs[REG_PB].u32 = b;
        exec_shader(gpu, gpu->fs_code_addr);
        color = (gpu->pRegs[REG_PR].u32 << 16) |
                (gpu->pRegs[REG_PG].u32 << 8)  |
                    gpu->pRegs[REG_PB].u32;
    }
    int idx = y * gpu->width + x;

    if (!gpu->blend_enable) {
        FB(gpu)[idx] = color;
        return;
    }

    float src_b = (color & 0xFF) / 255.0f;
    float src_g = ((color >> 8) & 0xFF) / 255.0f;
    float src_r = ((color >> 16) & 0xFF) / 255.0f;
    float src_a = ((color >> 24) & 0xFF) / 255.0f;

    uint32_t dst_u32 = FB(gpu)[idx];
    float dst_b = (dst_u32 & 0xFF) / 255.0f;
    float dst_g = ((dst_u32 >> 8) & 0xFF) / 255.0f;
    float dst_r = ((dst_u32 >> 16) & 0xFF) / 255.0f;
    float dst_a = ((dst_u32 >> 24) & 0xFF) / 255.0f;

    float f_src_r = get_blend_factor(gpu->blend_src_factor, src_a, dst_a, src_r, dst_r);
    float f_src_g = get_blend_factor(gpu->blend_src_factor, src_a, dst_a, src_g, dst_g);
    float f_src_b = get_blend_factor(gpu->blend_src_factor, src_a, dst_a, src_b, dst_b);

    float f_dst_r = get_blend_factor(gpu->blend_dst_factor, src_a, dst_a, src_r, dst_r);
    float f_dst_g = get_blend_factor(gpu->blend_dst_factor, src_a, dst_a, src_g, dst_g);
    float f_dst_b = get_blend_factor(gpu->blend_dst_factor, src_a, dst_a, src_b, dst_b);

    float out_r = src_r * f_src_r + dst_r * f_dst_r;
    float out_g = src_g * f_src_g + dst_g * f_dst_g;
    float out_b = src_b * f_src_b + dst_b * f_dst_b;
    float out_a = src_a + dst_a * (1.0f - src_a);

    uint8_t r = color_to_u8(out_r);
    uint8_t g = color_to_u8(out_g);
    uint8_t b = color_to_u8(out_b);
    uint8_t a = color_to_u8(out_a);

    FB(gpu)[idx] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
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
                    if(gpu->depth_write_enable == 1)
                    {
                        Z_BUFFER(gpu)[idx] = z;
                    }
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
        out_vertices[i].u = vertices[i].u;
        out_vertices[i].v = vertices[i].v;
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
            case TASK_RASTERIZE_POINTS_SIMT:   worker_rasterize_points_simt_impl(my_args); break;
            case TASK_RASTERIZE_LINES_SIMT:    worker_rasterize_lines_simt_impl(my_args); break;
            case TASK_WIREFRAME_VERTICES:      worker_wireframe_vertices_impl(my_args); break;
            case TASK_WIREFRAME_EDGES:         worker_wireframe_edges_impl(my_args); break;
            case TASK_COMPUTE_SIMT:            worker_compute_simt_impl(my_args); break;
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
    if (render.threads_initialized) return;

    render.work_generation = 0;
    render.completed_workers = 0;
    render.current_task = TASK_NONE;

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

void dispatch_task(RenderTaskType task) 
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

void gpu_render_primitives_simt(void *opaque, GpuPrimitiveType prim_type, float point_size, float line_width)
{
    GpuState *gpu = opaque;
    if (gpu->gpu_mode == GPU_MODE_IDLE)
    {
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;
    
    uint32_t vertex_size = gpu->vbo_config.size;
    if (vertex_size == 0)
    {
        gpu->gpu_mode = GPU_MODE_3D;
        return;
    }

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
        render.args[i].primitive_type = prim_type;
        render.args[i].point_size = point_size;
        render.args[i].line_width = line_width;
        
        render.args[i].start_block = i * chunk_v_blocks;
        render.args[i].end_block = (i == NUM_RENDER_THREADS - 1) ? total_vertex_blocks : (i + 1) * chunk_v_blocks;
    }
    dispatch_task(TASK_TRANSFORM_VERTICES_SIMT);

    // PASS 2: Primitive Assembly & Rasterization Dispatch
    uint32_t height = gpu->height;
    uint32_t chunk_y = height / NUM_RENDER_THREADS;
    chunk_y = (chunk_y + 3) & ~3; 

    if (prim_type == GPU_PRIM_POINTS)
    {
        for (int i = 0; i < NUM_RENDER_THREADS; i++) 
        {
            render.args[i].orig_gpu = gpu;
            render.args[i].transformed_vertices = transformed_vertices;
            render.args[i].primitive_type = prim_type;
            render.args[i].point_size = point_size;
            render.args[i].line_width = line_width;
            
            render.args[i].start_y = i * chunk_y;
            render.args[i].jit_ctx_fs = gpu->jit_ctx_fs;
            uint32_t end_y = (i == NUM_RENDER_THREADS - 1) ? height : (i + 1) * chunk_y;
            if (end_y > height) end_y = height;
            render.args[i].end_y = end_y;
        }
        dispatch_task(TASK_RASTERIZE_POINTS_SIMT);
    }
    else if (prim_type == GPU_PRIM_LINES || prim_type == GPU_PRIM_LINE_STRIP)
    {
        uint32_t line_count = 0;
        Edge *assembled_lines = assemble_primitive_lines(gpu, prim_type, vertex_size, &line_count);

        for (int i = 0; i < NUM_RENDER_THREADS; i++) 
        {
            render.args[i].orig_gpu = gpu;
            render.args[i].transformed_vertices = transformed_vertices;
            render.args[i].assembled_triangles = (Triangle *)assembled_lines;
            render.args[i].assembled_triangle_count = line_count;
            render.args[i].primitive_type = prim_type;
            render.args[i].point_size = point_size;
            render.args[i].line_width = line_width;
            
            render.args[i].start_y = i * chunk_y;
            render.args[i].jit_ctx_fs = gpu->jit_ctx_fs;
            uint32_t end_y = (i == NUM_RENDER_THREADS - 1) ? height : (i + 1) * chunk_y;
            if (end_y > height) end_y = height;
            render.args[i].end_y = end_y;
        }
        dispatch_task(TASK_RASTERIZE_LINES_SIMT);

        if (assembled_lines) free(assembled_lines);
    }
    else /* GPU_PRIM_TRIANGLES, GPU_PRIM_TRIANGLE_STRIP, GPU_PRIM_TRIANGLE_FAN, GPU_PRIM_QUADS */
    {
        uint32_t tri_count = 0;
        Triangle *assembled_triangles = assemble_primitive_triangles(gpu, prim_type, vertex_size, &tri_count);

        for (int i = 0; i < NUM_RENDER_THREADS; i++) 
        {
            render.args[i].orig_gpu = gpu;
            render.args[i].transformed_vertices = transformed_vertices;
            render.args[i].assembled_triangles = assembled_triangles;
            render.args[i].assembled_triangle_count = tri_count;
            render.args[i].primitive_type = prim_type;
            render.args[i].point_size = point_size;
            render.args[i].line_width = line_width;
            
            render.args[i].start_y = i * chunk_y;
            render.args[i].jit_ctx_fs = gpu->jit_ctx_fs;
            uint32_t end_y = (i == NUM_RENDER_THREADS - 1) ? height : (i + 1) * chunk_y;
            if (end_y > height) end_y = height;
            render.args[i].end_y = end_y;
        }
        dispatch_task(TASK_RASTERIZE_BANDS_SIMT);

        if (assembled_triangles) free(assembled_triangles);
    }

    free(transformed_vertices);
    gpu->gpu_mode = GPU_MODE_3D;
}

void gpu_render_triangles_simt(void *opaque)
{
    GpuState *gpu = opaque;
    float psize = gpu->point_size > 0.0f ? gpu->point_size : 1.0f;
    float lwidth = gpu->line_width > 0.0f ? gpu->line_width : 1.0f;
    GpuPrimitiveType prim = (gpu->primitive_type != 0) ? (GpuPrimitiveType)gpu->primitive_type : GPU_PRIM_TRIANGLES;
    gpu_render_primitives_simt(opaque, prim, psize, lwidth);
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

    TransformedVertex *transformed_vertices = malloc(sizeof(TransformedVertex) * vertex_size);
    if (!transformed_vertices)
    {
        gpu->gpu_mode = GPU_MODE_3D;
        return;
    }

    uint32_t chunk_v = vertex_size / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        render.args[i].orig_gpu = gpu;
        render.args[i].transformed_vertices = transformed_vertices;
        render.args[i].start_idx = i * chunk_v;
        render.args[i].end_idx = (i == NUM_RENDER_THREADS - 1) ? vertex_size : (i + 1) * chunk_v;
    }
    dispatch_task(TASK_TRANSFORM_VERTICES);

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