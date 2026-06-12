#ifndef RENDERER
#define RENDERER
#include "gpu.h"


#ifndef NUM_RENDER_THREADS
#define NUM_RENDER_THREADS 16
#endif

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
} RenderThreadArgs;


typedef enum {
    TASK_NONE = 0,
    TASK_TRANSFORM_VERTICES,
    TASK_RASTERIZE_BANDS,
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
void exec_shader(GpuState *gpu, uint32_t program_offset);

uint8_t cmp_u32(uint32_t a, uint32_t b, uint8_t flag);
uint8_t cmp_f32(float a, float b, uint8_t flag);
void gpu_render_wireframe(void *opaque);
void gpu_render_triangles(void *opaque);
float edge_func(Vec3 a, Vec3 b, Vec3 c);
void draw_triangle(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu);


#endif