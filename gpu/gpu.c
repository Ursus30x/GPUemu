// #define DEBUG_P_REG

#include "gpu.h"
#include "math3d.h"
#include "renderer.h"
#include "debug_gpu.h"

DECLARE_INSTANCE_CHECKER(GpuState, GPU, TYPE_PCI_GPU_DEVICE);

static void pci_gpu_register_types(void);
static void gpu_instance_init(Object *obj);
static void gpu_class_init(ObjectClass *class, const void *data);
static void pci_gpu_realize(PCIDevice *pdev, Error **errp);
static void pci_gpu_uninit(PCIDevice *pdev);
static void vga_update_display(void *opaque);

type_init(pci_gpu_register_types)

static void wireframe_3d_mode(GpuState *gpu)
{

    if (gpu->gpu_mode == GPU_MODE_3D)
    {
        gpu_render_wireframe(gpu);
        vga_update_display(gpu);
        graphic_hw_update(gpu->con);
    }
}
static void triangles_3d_mode(GpuState *gpu)
{

    if (gpu->gpu_mode == GPU_MODE_3D)
    {
        gpu_render_triangles(gpu);
        vga_update_display(gpu);
        graphic_hw_update(gpu->con);
    }
}
static void gpu_print_mmio(GpuState *s)
{
    DEBUG_PRINT("\n--- GPU MMIO Register Snapshot ---\n");
    DEBUG_PRINT("0x00 [GPU_MODE]:         0x%08x\n", s->gpu_mode);
    DEBUG_PRINT("0x04 [RING_HEAD]:        0x%08x\n", s->ring_buffer_head);
    DEBUG_PRINT("0x08 [RING_TAIL]:        0x%08x\n", s->ring_buffer_tail);
    DEBUG_PRINT("0x10 [VS_CODE_PTR]:      0x%08x\n", s->vs_code_addr);
    DEBUG_PRINT("0x14 [FS_CODE_PTR]:      0x%08x\n", s->fs_code_addr);
    DEBUG_PRINT("0x18 [WIDTH]:            %u\n", s->width);
    DEBUG_PRINT("0x1C [HEIGHT]:           %u\n", s->height);
    DEBUG_PRINT("0x20 [FB_ADDR]:          0x%08x\n", s->framebuffer_vram_offset);
    DEBUG_PRINT("0x24 [GPU_TIME]:         %u\n", s->gpu_time);
    DEBUG_PRINT("0x2C [ZBUFFER]:          0x%08x\n", s->zbuffer_addr);

    DEBUG_PRINT("----------------------------------\n");
}

static void update_32bit_register(uint32_t *target_ptr, hwaddr offset_in_word, uint64_t val, unsigned size)
{
    uint32_t clear_mask = 0xFFFFFFFF;
    uint32_t data_mask = (size == 4) ? 0xFFFFFFFF : ((1U << (size * 8)) - 1);

    clear_mask ^= (data_mask << (offset_in_word * 8));
    *target_ptr &= clear_mask;
    *target_ptr |= ((uint32_t)val & data_mask) << (offset_in_word * 8);
}

static void execute_command(GpuState *gpu, Command *cmd)
{
    switch (cmd->opcode)
    {
    case CMD_CLEAR_FRAMEBUFFER:
        DEBUG_PRINT("[CMD] Clear FB %x \n", (gpu->width * gpu->height));
        for (uint32_t i = 0; i < (gpu->width * gpu->height); i++)
        {
            FB(gpu)[i] = 0xff000000;
            Z_BUFFER(gpu)[i] = FLT_MAX;
        }
        break;
    case CMD_SET_STATE:
        DEBUG_PRINT("[CMD] Set state\n");
        switch (cmd->payload.state.state_id)
        {
        case STATE_ID_EDGE_CONFIG:
            DEBUG_PRINT("[CMD] Edge config\n");
            gpu->edge_config = cmd->payload.state.value.buffer_config;
            break;
        case STATE_ID_VBO_CONFIG:
            DEBUG_PRINT("[CMD] VBO config\n");
            gpu->vbo_config = cmd->payload.state.value.buffer_config;
            break;
        case STATE_ID_UNIFORM_CONFIG:
            DEBUG_PRINT("[CMD] UBO config\n");
            gpu->uinform_config = cmd->payload.state.value.buffer_config;
            break;
        case STATE_ID_VERTEX_SHADER_PTR:
            DEBUG_PRINT("[CMD] Vertex shader config\n");
            gpu->vs_code_addr = cmd->payload.state.value.shader_ptrs.vs_addr;
            break;
        case STATE_ID_FRAGMENT_SHADER_PTR:
            DEBUG_PRINT("[CMD] Fragment shader config\n");
            gpu->fs_code_addr = cmd->payload.state.value.shader_ptrs.fs_addr;
            break;
        default:
            break;
        }
        break;
    case CMD_DRAW_PRIMITIVE:
        if(cmd->payload.draw.type == PRIMITIVE_TYPE_LINES)
            wireframe_3d_mode(gpu);
        else if(cmd->payload.draw.type == PRIMITIVE_TYPE_TRIANGLES)
            triangles_3d_mode(gpu);
        //printf("[CMD] Draw primitive\n");
        break;
    case CMD_NOOP:
        DEBUG_PRINT("[CMD] NOP\n");
    default:
        break;
    }
}

static void process_ring_buffer(GpuState *s)
{
    // 1. Read Dynamic Configuration directly from State
    uint32_t rb_start = s->ring_buffer_start;
    uint32_t rb_end   = s->ring_buffer_end;
    uint32_t head     = s->ring_buffer_head; 

    // 2. Safety: Driver hasn't initialized registers yet
    if (rb_start == 0 || rb_end <= rb_start) {
        return;
    }

    // 3. Dynamic Bounds Check (Anti-Segfault)
    // If tail is outside valid range (e.g. after driver restart), reset it.
    if (s->ring_buffer_tail < rb_start || s->ring_buffer_tail >= rb_end) {
        DEBUG_PRINT("[GPU ERROR] Tail (0x%x) out of bounds [0x%x - 0x%x]. Resetting to Start.\n", 
            s->ring_buffer_tail, rb_start, rb_end);
        s->ring_buffer_tail = rb_start;
    }

    // 4. Idle Check
    if (head == s->ring_buffer_tail) {
        return;
    }

    // 5. Processing Loop
    while (s->ring_buffer_tail != head)
    {
        uint8_t *cmd_ptr = s->vram_ptr + s->ring_buffer_tail;
        Command *cmd = (Command *)cmd_ptr;

        execute_command(s, cmd);

        s->ring_buffer_tail += sizeof(Command);

        // --- DYNAMIC WRAP LOGIC ---
        // Wrap exactly at the configured End address
        if (s->ring_buffer_tail >= rb_end) {
            DEBUG_PRINT("[GPU CMD] Buffer End Reached. Wrapping to 0x%x\n", rb_start);
            s->ring_buffer_tail = rb_start;
        }
    }
}

static void gpu_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    GpuState *s = (GpuState *)opaque;

    hwaddr offset_in_word = addr % 4;
    hwaddr base_addr = addr - offset_in_word;

    uint32_t *target_reg = NULL;
    uint8_t trigger_command_processor = 0;

    switch (base_addr)
    {
    case REG_GPU_MODE_ADDR:
        target_reg = &s->gpu_mode;
        break;
    case REG_RING_BUFFER_HEAD_ADDR:
        target_reg = &s->ring_buffer_head;
        if(s->gpu_mode != GPU_MODE_GOP) trigger_command_processor = 1;
        break;
    case REG_RING_BUFFER_TAIL_ADDR:
        target_reg = &s->ring_buffer_tail;
        break;
    case REG_RING_BUFFER_START_ADDR:
        target_reg = &s->ring_buffer_start;
        break;
    case REG_RING_BUFFER_END_ADDR: 
        target_reg = &s->ring_buffer_end;
        break;
    case REG_VERTEX_SHADER_ADDR:
        target_reg = &s->vs_code_addr;
        break;
    case REG_FRAGMENT_SHADER_ADDR:
        target_reg = &s->fs_code_addr;
        break;
    case REG_FB_WIDTH_ADDR:
        target_reg = &s->width;
        break;
    case REG_FB_HEIGHT_ADDR:
        target_reg = &s->height;
        break;
    case REG_FRAMEBUFFER_ADDR:
        target_reg = &s->framebuffer_vram_offset;
        break;
    case REG_ZBUFFER_ADDR:
         target_reg = &s->zbuffer_addr;
        break;
    case REG_GPU_TIME_ADDR:
        fprintf(stderr, "GPU MMIO WRITE: Warning: Attempted write to read-only GPU_TIME at 0x%" PRIx64 "\n", addr);
        return;

    default:
        fprintf(stderr, "GPU MMIO WRITE: Unhandled base offset 0x%" PRIx64 " (val: 0x%lx, size: %u)\n", base_addr, val, size);
        return;
    }

    if (target_reg)
    {
        if (offset_in_word + size > 4)
        {
            fprintf(stderr, "GPU MMIO WRITE: Misaligned write crossing 32-bit boundary at 0x%" PRIx64 "\n", addr);
            return;
        }

        update_32bit_register(target_reg, offset_in_word, val, size);
        gpu_print_mmio(s);
        DEBUG_PRINT("GPU MMIO WRITE: 0x%lx written to 0x%" PRIx64 " (size %u). New reg value: 0x%x\n", val, addr, size, *target_reg);
        if (trigger_command_processor)
        {
            DEBUG_PRINT("\n[GPU TRIGGER] Detected write to RING_HEAD (0x04). Launching command processor...\n");
            process_ring_buffer(s);
        }
    }
}

static uint64_t gpu_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    GpuState *s = (GpuState *)opaque;
    uint32_t reg_val = 0;

    hwaddr offset_in_word = addr % 4;
    hwaddr base_addr = addr - offset_in_word;

    switch (base_addr)
    {
    case REG_GPU_MODE_ADDR:
        reg_val = s->gpu_mode;
        break;
    case REG_RING_BUFFER_HEAD_ADDR:
        reg_val = s->ring_buffer_head;
        break;
    case REG_RING_BUFFER_TAIL_ADDR:
        reg_val = s->ring_buffer_tail;
        break;
    case REG_VERTEX_SHADER_ADDR:
        reg_val = s->vs_code_addr;
        break;
    case REG_FRAGMENT_SHADER_ADDR:
        reg_val = s->fs_code_addr;
        break;
    case REG_FB_WIDTH_ADDR:
        reg_val = s->width;
        break;
    case REG_FB_HEIGHT_ADDR:
        reg_val = s->height;
        break;
    case REG_FRAMEBUFFER_ADDR:
        reg_val = s->framebuffer_vram_offset;
        break;
    case REG_GPU_TIME_ADDR:
        reg_val = s->gpu_time;
        break;
    case REG_ZBUFFER_ADDR:
        reg_val = s->zbuffer_addr;
        break;
    default:
        fprintf(stderr, "GPU MMIO READ: Unhandled base offset 0x%" PRIx64 "\n", base_addr);
        return 0;
    }

    uint64_t result = (reg_val >> (offset_in_word * 8)) & ((1ULL << (size * 8)) - 1);

    DEBUG_PRINT("GPU MMIO READ: 0x%" PRIx64 " returned from offset 0x%" PRIx64 " (size %u)\n", result, addr, size);
    return result;
}

static const MemoryRegionOps gpu_mmio_ops = {
    .read = gpu_mmio_read,
    .write = gpu_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void pci_gpu_register_types(void)
{
    static InterfaceInfo interfaces[] = {
        {INTERFACE_CONVENTIONAL_PCI_DEVICE},
        {},
    };

    static const TypeInfo gpu_info = {
        .name = TYPE_PCI_GPU_DEVICE,
        .parent = TYPE_PCI_DEVICE,
        .instance_size = sizeof(GpuState),
        .instance_init = gpu_instance_init,
        .class_init = gpu_class_init,
        .interfaces = interfaces,
    };

    type_register_static(&gpu_info);
}

static void gpu_instance_init(Object *obj)
{
}
static void vga_invalidate_display(void *opaque)
{
    DEBUG_PRINT("invalidated display\n");
}
static void vga_update_text(void *opaque, console_ch_t *chardata)
{
    DEBUG_PRINT("updated text\n");
}

static void vga_update_display(void *opaque)
{
    GpuState *gpu = opaque;

    uint32_t width = gpu->width;
    uint32_t height = gpu->height;
    DisplaySurface *surface = qemu_console_surface(gpu->con);
    for (uint32_t i = 0; i < width * height; i++)
        ((uint32_t *)surface_data(surface))[i] = FB(gpu)[i];
    dpy_gfx_update(gpu->con, 0, 0, 640, 480);
}

static const GraphicHwOps ghwops = {
    .invalidate = vga_invalidate_display,
    .gfx_update = vga_update_display,
    .text_update = vga_update_text,
};

static void gpu_class_init(ObjectClass *class, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

    k->realize = pci_gpu_realize;
    k->exit = pci_gpu_uninit;
    k->vendor_id = PCI_VENDOR_ID_CUSTOM;
    k->device_id = GPU_DEVICE_ID;
    k->revision = 0x01;
    k->class_id = PCI_CLASS_DISPLAY_OTHER;
}

/* Realize GPU device */
static void pci_gpu_realize(PCIDevice *pdev, Error **errp)
{
    DEBUG_PRINT("pci_gpu_realize\n");

    GpuState *gpu = GPU(pdev);
    /* BAR0: cmd registers */
    memory_region_init_io(&gpu->mmiomem, OBJECT(gpu), &gpu_mmio_ops, gpu, "gpu-mmio", GPU_CMD_SIZE);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &gpu->mmiomem);

    /* BAR1:  VRAM */
    memory_region_init_ram(&gpu->vrammem, OBJECT(gpu), "gpu-vram", GPU_VRAM_SIZE, errp);
    pci_register_bar(pdev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &gpu->vrammem);

    gpu->vram_ptr = memory_region_get_ram_ptr(&gpu->vrammem);

    gpu->con = graphic_console_init(DEVICE(pdev), 0, &ghwops, gpu);

    // init state
    gpu->height = 480;
    gpu->width = 640;
    gpu->gpu_mode = GPU_MODE_GOP;
    gpu->framebuffer_vram_offset = 0x0000000;

    uint64_t bin_shader[] = { 0x84100090B, 0x44200000, 0x94101090B, 0x43F00000, 0x41000901, 0x40000000, 0x4100090A, 0x3F800000, 0x141010901, 0x40000000, 0x14101090A, 0x3F800000, 0x41000901, 0x3FAAAAAA, 0x14502092D, 0x0, 0x145010926, 0x0, 0x40000916, 0x0, 0x141030901, 0x41200000, 0x41040901, 0x40000000, 0x34503090A, 0x4, 0x341030913, 0x0, 0x241040901, 0x41200000, 0x41050901, 0x40400000, 0x445050909, 0x5, 0x541050914, 0x0, 0x345050909, 0x5, 0x545060909, 0x0, 0x64106092C, 0x0, 0x41070901, 0x3F000000, 0x54507090A, 0x7, 0x74107092C, 0x0, 0x41030901, 0x3E99999A, 0x541050901, 0x3F333333, 0x345080909, 0x5, 0x84108092C, 0x0, 0x61500092B, 0x800000007, 0x1000901, 0x3F000000, 0x1000909, 0x3F000000, 0x141010921, 0x0, 0x141010901, 0x40400000, 0x14102092E, 0x0, 0x5000901, 0x2, 0x90D, 0x0, 0x907, 0x0 };
    size_t shader_size = sizeof(bin_shader);
    gpu->pRegs[REG_PX].f32 = 320.0f;
    gpu->pRegs[REG_PY].f32 = 100.0f;
    memcpy(gpu->vram_ptr + 0xb00000, bin_shader, shader_size);
    exec_shader(gpu, 0xb00000);
}

/* Uninitialize GPU device */
static void pci_gpu_uninit(PCIDevice *pdev)
{
}
