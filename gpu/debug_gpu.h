#include "gpu.h"
//#define DEBUG
#define DEBUG_VAR __attribute__((unused))
#ifdef DEBUG
  #define DEBUG_PRINT(...) printf(__VA_ARGS__)
  #define DEDUG_MAT

static void G_GNUC_UNUSED debug_dump_vertices(GpuState *gpu)
{
    // Resolve the pointer using your macro
    Vec3 *vertices = VERTEX_TABLE(gpu);
    
    uint32_t count = gpu->vbo_config.size; 

    DEBUG_PRINT("--- [DEBUG] VERTEX TABLE (Addr: %x, Count: %u) ---\n", 
           gpu->vbo_config.addr, count);

    if (count == 0 || count > 1000) { // Sanity check
        DEBUG_PRINT("[WARN] Vertex count suspicious. Aborting dump.\n");
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        // Print floats and Hex color
        DEBUG_PRINT("  [%02u] X:%7.3f Y:%7.3f Z:%7.3f | Color: 0x%08X\n", 
               i, vertices[i].x, vertices[i].y, vertices[i].z, vertices[i].rgba);
    }
    DEBUG_PRINT("-------------------------------------------------------------\n");
}

static void G_GNUC_UNUSED debug_dump_edges(GpuState *gpu)
{
    Edge *edges = EDGES_TABLE(gpu);
    
    uint32_t count = gpu->edge_config.size;

    DEBUG_PRINT("--- [DEBUG] EDGE TABLE (Addr: %x, Count: %u) ---\n", 
           gpu->edge_config.addr, count);

    if (count == 0 || count > 1000) {
        DEBUG_PRINT("[WARN] Edge count suspicious. Aborting dump.\n");
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        DEBUG_PRINT("  [%02u] A: %u -> B: %u\n", i, edges[i].a, edges[i].b);
    }
    DEBUG_PRINT("----------------------------------------------------------\n");
}

static void G_GNUC_UNUSED debug_dump_ubo(void *opaque)
{
    GpuState *gpu = (GpuState*)opaque;
    
    uint32_t addr = gpu->uinform_config.addr; 
    uint32_t size = gpu->uinform_config.size;
    uint8_t *vram_base = (uint8_t*)gpu->vram_ptr;
    
    // Guard against uninitialized config
    if (size == 0) {
        DEBUG_PRINT("--- [DEBUG] UBO DATA (Empty / Size 0) ---\n");
        return;
    }

    DEBUG_PRINT("--- [DEBUG] UBO DATA (Addr: 0x%x, Bytes: %u) ---\n", addr, size);

    // Iterate in 16-byte steps (Size of Vec4)
    for(uint32_t i = 0; i < size; i += 16)
    {
        float *f_vals = (float*)(vram_base + addr + i);
        uint32_t *u_vals = (uint32_t*)(vram_base + addr + i);

        float v[4] = {0};
        uint32_t h[4] = {0};
        
        for(int k=0; k<4; k++) {
            if (i + (k * 4) < size) {
                v[k] = f_vals[k];
                h[k] = u_vals[k];
            }
        }

        DEBUG_PRINT("  [0x%03x] %8.3f %8.3f %8.3f %8.3f | %08X %08X %08X %08X\n", 
            i, 
            v[0], v[1], v[2], v[3],
            h[0], h[1], h[2], h[3]    
        );
    }
    DEBUG_PRINT("----------------------------------------------------------\n");
}
#else
  #define DEBUG_PRINT(...) ;

__attribute__((unused))
static void debug_dump_vertices(GpuState *gpu){}

__attribute__((unused))
static void debug_dump_edges(GpuState *gpu){}

__attribute__((unused))
static void debug_dump_ubo(void *opaque){}

#endif