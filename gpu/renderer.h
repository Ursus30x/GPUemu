#ifndef RENDERER
#define RENDERER
#include "gpu.h"


#ifndef NUM_RENDER_THREADS
#define NUM_RENDER_THREADS 32
#endif
extern QemuMutex jit_mutex;

typedef struct {
    Vec4 pos;
    uint32_t color;
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

#define GET_R(color) (uint8_t)(((color) >> 16) & 0xFF)
#define GET_G(color) (uint8_t)(((color) >> 8) & 0xFF)
#define GET_B(color) (uint8_t)((color) & 0xFF)

#define RGB_TO_UINT(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))


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