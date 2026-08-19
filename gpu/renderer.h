#ifndef RENDERER
#define RENDERER
#include "gpu.h"


#ifndef NUM_RENDER_THREADS
#define NUM_RENDER_THREADS 16
#endif
extern QemuMutex jit_mutex;
#include <stdbool.h>

typedef struct {
    Vec3 s[3];
    float s_inv_w[3];
    float inv_z[3];

    int min_x, max_x, min_y, max_y;
    int stamp_min_x, stamp_max_x, stamp_min_y, stamp_max_y;

    float d_w0_dx, d_w0_dy;
    float d_w1_dx, d_w1_dy;
    float d_w2_dx, d_w2_dy;
    float d_w3_dx, d_w3_dy;

    float start_w0, start_w1, start_w2, start_w3;

    float r_inv_w[3], g_inv_w[3], b_inv_w[3], a_inv_w[3];
    float u_inv_w[3], v_inv_w[3];  /* perspective-correct UV interpolants */
} TriangleContext;


typedef struct {
    Vec4 pos;
    uint32_t color;
    float u, v;       /* per-vertex texture coordinates */
} TransformedVertex;

typedef struct {
    GpuState *orig_gpu;
    uint32_t start_idx;
    uint32_t end_idx;
    uint32_t *px;
    uint32_t *py;
    TransformedVertex *transformed_vertices;
    uint32_t triangle_size;
    uint32_t start_y;
    uint32_t end_y;
    uint32_t start_block;
    uint32_t end_block;
    uint16_t raster_exec_mask;
    SimtVec4 transformed_simt;
    SimtVec2 transformed_uv_simt;

    JitContext jit_ctx_vs; 
    JitContext jit_ctx_fs; 
} RenderThreadArgs;


typedef enum {
    TASK_NONE = 0,
    TASK_TRANSFORM_VERTICES,
    TASK_TRANSFORM_VERTICES_SIMT,
    TASK_RASTERIZE_BANDS,
    TASK_RASTERIZE_BANDS_SIMT,
    TASK_WIREFRAME_VERTICES,
    TASK_WIREFRAME_EDGES,
    TASK_EXIT
} RenderTaskType;

typedef struct {
    QemuThread threads[NUM_RENDER_THREADS];
    RenderThreadArgs args[NUM_RENDER_THREADS];
    QemuMutex pool_mutex;
    QemuCond pool_cond;
    QemuCond main_cond;

    uint32_t work_generation;
    uint32_t completed_workers;
    RenderTaskType current_task;
    uint32_t threads_initialized;
} RendererThreads;

#define GET_A(color) ((uint8_t)(((color) >> 24) & 0xFF))
#define GET_R(color) (uint8_t)(((color) >> 16) & 0xFF)
#define GET_G(color) (uint8_t)(((color) >> 8) & 0xFF)
#define GET_B(color) (uint8_t)((color) & 0xFF)

#define RGB_TO_UINT(r, g, b)     (((uint32_t)(0xFF) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define RGBA_TO_UINT(r, g, b, a)     (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

// UBO access macro: Get pointer to UBO data in VRAM
#define UBO_DATA(gpu) (gpu->uinform_config.size > 0 ? (gpu->vram_ptr + gpu->uinform_config.addr) : NULL)

// Texture descriptor access macro: Get pointer to Texture Descriptor in VRAM
#define TEXTURE_DESC_DATA(gpu, slot) (((slot) < MAX_BINDINGS && (gpu)->texture_desc_addr[slot] > 0) ? (GpuTextureDescriptorVram*)((gpu)->vram_ptr + (gpu)->texture_desc_addr[slot]) : NULL)


void init_thread_pool(void);
void put_pixel(GpuState *gpu, int x, int y, uint32_t color);
void draw_line(GpuState *gpu, int x0, int y0, int x1, int y1, uint32_t color1, uint32_t color2);

void gpu_render_wireframe(void *opaque);
void gpu_render_triangles(void *opaque);
float edge_func(Vec3 a, Vec3 b, Vec3 c);
void draw_triangle(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu);
void gpu_render_triangles_simt(void *opaque);
void worker_transform_vertices_simt_impl(RenderThreadArgs *args) ;
#endif