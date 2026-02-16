#include "gpu.h"
#include "jit.h"
#define DEBUG
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
static void debug_print_single_deco(uint32_t id, SpvDecoInfo* d)
{
    if (!d || !d->is_decorated)
        return;

    DEBUG_PRINT("ID %u Decorations:\n", id);

    if (d->descriptor_set >= 0)
        DEBUG_PRINT("  DescriptorSet: %d\n", d->descriptor_set);

    if (d->binding >= 0)
        DEBUG_PRINT("  Binding: %d\n", d->binding);

    if (d->location >= 0)
        DEBUG_PRINT("  Location: %d\n", d->location);

    if (d->builtin >= 0)
        DEBUG_PRINT("  BuiltIn: %d\n", d->builtin);

    if (d->array_stride >= 0)
        DEBUG_PRINT("  ArrayStride: %d\n", d->array_stride);

    DEBUG_PRINT("\n");
}

static void debug_dump_decorations(JitContext* ctx)
{
    DEBUG_PRINT("==== ID Decorations ====\n");

    for (uint32_t i = 0; i < ctx->bound; i++)
    {
        if (ctx->decorations[i].is_decorated)
            debug_print_single_deco(i, &ctx->decorations[i]);
    }
}
/* ============================================================
   Member Decorations Debug
   ============================================================ */

static void G_GNUC_UNUSED debug_print_member_list(uint32_t struct_id, MemberDecoNode* node)
{
    while (node)
    {
        DEBUG_PRINT("Struct %u Member %u:\n",
                    struct_id, node->member_index);

        if (node->offset >= 0)
            DEBUG_PRINT("  Offset: %d\n", node->offset);

        if (node->matrix_stride >= 0)
            DEBUG_PRINT("  MatrixStride: %d\n", node->matrix_stride);

        DEBUG_PRINT("\n");

        node = node->next;
    }
}

static void G_GNUC_UNUSED debug_dump_member_decorations(JitContext* ctx)
{
    DEBUG_PRINT("==== Member Decorations ====\n");

    for (uint32_t i = 0; i < ctx->bound; i++)
    {
        if (ctx->member_decorations[i])
            debug_print_member_list(i, ctx->member_decorations[i]);
    }
}


/* ============================================================
   Type Info Debug
   ============================================================ */

static const char* G_GNUC_UNUSED spv_opcode_to_string(SpvOp op)
{
    switch (op)
    {
        case SpvOpTypeStruct:   return "OpTypeStruct";
        case SpvOpTypePointer:  return "OpTypePointer";
        case SpvOpTypeArray:    return "OpTypeArray";
        case SpvOpTypeFloat:    return "OpTypeFloat";
        case SpvOpTypeInt:      return "OpTypeInt";
        default:                return "Other";
    }
}

static void G_GNUC_UNUSED debug_dump_type_info(JitContext* ctx)
{
    DEBUG_PRINT("==== Type Info ====\n");

    for (uint32_t i = 0; i < ctx->bound; i++)
    {
        SpvTypeInfo* t = &ctx->type_info[i];

        if (!t->opcode)
            continue;

        DEBUG_PRINT("Type ID %u:\n", i);
        DEBUG_PRINT("  Opcode: %s\n", spv_opcode_to_string(t->opcode));
        DEBUG_PRINT("  Base Type ID: %u\n", t->base_type_id);

        if (t->member_count > 0 && t->member_types)
        {
            DEBUG_PRINT("  Members (%u): ", t->member_count);
            for (uint32_t m = 0; m < t->member_count; m++)
                DEBUG_PRINT("%u ", t->member_types[m]);
            DEBUG_PRINT("\n");
        }

        DEBUG_PRINT("\n");
    }
}


/* ============================================================
   Full Metadata Dump
   ============================================================ */

static void G_GNUC_UNUSED debug_dump_all_metadata(JitContext* ctx)
{
    DEBUG_PRINT("\n====================================\n");
    DEBUG_PRINT("        SPIR-V METADATA DUMP        \n");
    DEBUG_PRINT("====================================\n\n");

    debug_dump_decorations(ctx);
    debug_dump_member_decorations(ctx);
    debug_dump_type_info(ctx);

    DEBUG_PRINT("====================================\n\n");
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