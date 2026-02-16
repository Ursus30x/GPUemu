// #define DEBUG_P_REG

#include "gpu.h"
#include "math3d.h"
#include "renderer.h"
#include "debug_gpu.h"
#include "jit.h"

DECLARE_INSTANCE_CHECKER(GpuState, GPU, TYPE_PCI_GPU_DEVICE);

static void pci_gpu_register_types(void);
static void gpu_instance_init(Object *obj);
static void gpu_class_init(ObjectClass *class, const void *data);
static void pci_gpu_realize(PCIDevice *pdev, Error **errp);
static void pci_gpu_uninit(PCIDevice *pdev);
static void vga_update_display(void *opaque);
static void handle_dma(GpuState *s);
static void process_ring_buffer(GpuState *s);

/* Thread Worker Functions */
static void *gpu_refresh_thread_worker(void *opaque)
{
    GpuState *s = opaque;
    while (1) {
        // ~60 FPS refresh rate
        g_usleep(16666); 

        if (s->threads_exit) {
            break;
        }

        // Fetch data from GPU VRAM to internal QEMU buffer
        qemu_mutex_lock(&s->render_mutex);
        uint32_t *fb_ptr = FB(s);
        if (fb_ptr) {
            memcpy(s->internal_fb, fb_ptr, s->width * s->height * 4);
        }
        qemu_mutex_unlock(&s->render_mutex);

        // Signal QEMU Main Loop to update the screen
        bql_lock();
        dpy_gfx_update(s->con, 0, 0, s->width, s->height);
        bql_unlock();
    }
    return NULL;
}

static void *gpu_dma_thread_worker(void *opaque)
{
    GpuState *s = opaque;
    while (1) {
        qemu_mutex_lock(&s->dma_mutex);
        while (!s->dma_pending && !s->threads_exit) {
            qemu_cond_wait(&s->dma_cond, &s->dma_mutex);
        }
        if (s->threads_exit) {
            qemu_mutex_unlock(&s->dma_mutex);
            break;
        }
        s->dma_pending = false;
        qemu_mutex_unlock(&s->dma_mutex);

        handle_dma(s);
        
        // Use atomic or lock if dma_cmd needs protection, but for now MMIO write is sync with main loop
        s->dma_cmd &= ~GPU_DMA_CMD_START;
    }
    return NULL;
}

static void *gpu_cmd_thread_worker(void *opaque)
{
    GpuState *s = opaque;
    while (1) {
        qemu_mutex_lock(&s->cmd_mutex);
        while (!s->cmd_pending && !s->threads_exit) {
            qemu_cond_wait(&s->cmd_cond, &s->cmd_mutex);
        }
        if (s->threads_exit) {
            qemu_mutex_unlock(&s->cmd_mutex);
            break;
        }
        s->cmd_pending = false;
        qemu_mutex_unlock(&s->cmd_mutex);

        process_ring_buffer(s);
    }
    return NULL;
}

type_init(pci_gpu_register_types)

static void wireframe_3d_mode(GpuState *gpu)
{

    if (gpu->gpu_mode == GPU_MODE_3D)
    {
        gpu_render_wireframe(gpu);
    }
}
static void triangles_3d_mode(GpuState *gpu)
{

    if (gpu->gpu_mode == GPU_MODE_3D)
    {
        gpu_render_triangles(gpu);
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

static void handle_dma(GpuState *s)
{
    PCIDevice *pdev = PCI_DEVICE(s);

    // Check if bus mastering is enabled
    if (!(pdev->config[PCI_COMMAND] & PCI_COMMAND_MASTER)) {
        fprintf(stderr, "GPU DMA: Error: Driver attempted DMA while Bus Mastering is disabled!\n");
        return;
    }

    // Read MMIO registers
    const uint8_t direction = (s->dma_cmd >> 1) & 1;
    const uint32_t host_addr = s->dma_addr;
    const uint32_t vram_offset = s->dma_vram;
    const uint32_t size = s->dma_size;


    if (vram_offset + size > GPU_VRAM_SIZE) {
        fprintf(stderr, "GPU DMA: Error: Transfer exceeds VRAM size!\n");
        return;
    }

    // Determine transfer direction
    if (direction == GPU_DMA_CMD_FROM_VRAM)
    {
        DEBUG_PRINT("GPU DMA: VRAM(0x%x) -> Host(0x%x), size 0x%x\n", vram_offset, host_addr, size);
        pci_dma_write(pdev, host_addr, s->vram_ptr + vram_offset, size);
    }
    else if(direction == GPU_DMA_CMD_TO_VRAM){
        DEBUG_PRINT("GPU DMA: Host(0x%x) -> VRAM(0x%x), size 0x%x\n", host_addr, vram_offset, size);
        pci_dma_read(pdev, host_addr, s->vram_ptr + vram_offset, size);
    }

    s->int_status |= GPU_INT_DMA_DONE;

    // Notify host with interrupt if unmasked
    if (s->int_mask & GPU_INT_DMA_DONE) {
        if (msi_enabled(pdev)) {
            bql_lock();
            msi_notify(pdev, 0);
            bql_unlock();
        }
    }
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

    case CMD_DMA_TRANSFER:
        DEBUG_PRINT("[CMD] Queued DMA Transfer\n");
        gpu->dma_addr = cmd->payload.dma.host_addr;
        gpu->dma_vram = cmd->payload.dma.vram_offset;
        gpu->dma_size = cmd->payload.dma.size;
        gpu->dma_cmd  = cmd->payload.dma.cmd;
        handle_dma(gpu);
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
    // Read state of the ring buffer regs
    uint32_t rb_start = s->ring_buffer_start;
    uint32_t rb_end   = s->ring_buffer_end;
    uint32_t head     = s->ring_buffer_head;
    uint32_t tail     = s->ring_buffer_tail;

    // Check if ring buffer is initalized
    if (rb_start == 0 || rb_end <= rb_start) {
        return;
    }

    // Check if rb is within bounds (sanity check)
    if (tail < rb_start || tail >= rb_end) {
        DEBUG_PRINT("[GPU ERROR] Tail (0x%x) out of bounds [0x%x - 0x%x]. Resetting to Start.\n",
            tail, rb_start, rb_end);
        tail = rb_start;
    }

    // If both head and tail are at the same address then rb is idle
    if (head == tail) {
        return;
    }

    // Process commands
    qemu_mutex_lock(&s->render_mutex);
    while (tail != head)
    {
        uint8_t *cmd_ptr = s->vram_ptr + tail;
        Command *cmd = (Command *)cmd_ptr;

        execute_command(s, cmd);

        tail += sizeof(Command);

        // --- DYNAMIC WRAP LOGIC ---
        if (tail >= rb_end) {
            DEBUG_PRINT("[GPU CMD] Buffer End Reached. Wrapping to 0x%x\n", rb_start);
            tail = rb_start;
        }
    }
    qemu_mutex_unlock(&s->render_mutex);

    // Update the shared register ONLY once we are finished
    s->ring_buffer_tail = tail;
    s->int_status |= GPU_INT_CMD_DONE;

    // Notify via MSI
    if (s->int_mask & GPU_INT_CMD_DONE) {
        if (msi_enabled(PCI_DEVICE(s))) {
            bql_lock();
            msi_notify(PCI_DEVICE(s), 0);
            bql_unlock();
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
    uint8_t trigger_dma = 0;

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
    case REG_INT_ACK_ADDR:
        s->int_status &= ~val;
        return;
    case REG_INT_MASK_ADDR:
        target_reg = &s->int_mask;
        break;
    case REG_DMA_HOST_ADDR:
        target_reg = &s->dma_addr;
        break;
    case REG_DMA_VRAM_ADDR:
        target_reg = &s->dma_vram;
        break;
    case REG_DMA_SIZE_ADDR:
        target_reg = &s->dma_size;
        break;
    case REG_DMA_CMD_ADDR:
        target_reg = &s->dma_cmd;
        trigger_dma = 1;
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
            DEBUG_PRINT("\n[GPU TRIGGER] Detected write to RING_HEAD (0x04). Signaling command processor thread...\n");
            qemu_mutex_lock(&s->cmd_mutex);
            s->cmd_pending = true;
            qemu_cond_signal(&s->cmd_cond);
            qemu_mutex_unlock(&s->cmd_mutex);
        }
        if (trigger_dma && (*target_reg & GPU_DMA_CMD_START))
        {
            DEBUG_PRINT("\n[GPU TRIGGER] DMA_CMD START detected. Signaling DMA transfer thread...\n");
            qemu_mutex_lock(&s->dma_mutex);
            s->dma_pending = true;
            qemu_cond_signal(&s->dma_cond);
            qemu_mutex_unlock(&s->dma_mutex);
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
    case REG_RING_BUFFER_START_ADDR:
        reg_val = s->ring_buffer_start;
        break;
    case REG_RING_BUFFER_END_ADDR:
        reg_val = s->ring_buffer_end;
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
    case REG_INT_STATUS_ADDR:
        reg_val = s->int_status;
        break;
    case REG_INT_MASK_ADDR:
        reg_val = s->int_mask;
        break;
    case REG_DMA_HOST_ADDR:
        reg_val = s->dma_addr;
        break;
    case REG_DMA_VRAM_ADDR:
        reg_val = s->dma_vram;
        break;
    case REG_DMA_SIZE_ADDR:
        reg_val = s->dma_size;
        break;
    case REG_DMA_CMD_ADDR:
        reg_val = s->dma_cmd;
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

    // No lock needed here as we read from the internal shadow buffer
    // which was populated by the refresh thread.
    for (uint32_t i = 0; i < width * height; i++)
        ((uint32_t *)surface_data(surface))[i] = gpu->internal_fb[i];
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


    //DEBUG JITTER 

   // SPIR-V Shader Data (548 bytes)
    uint32_t code[] = {
    0x07230203, 0x00010000, 0x00070000, 0x00000010, 0x00000000, 0x00020011, 0x00000001, 0x0003000e, 
    0x00000000, 0x00000001, 0x0005000f, 0x00000005, 0x00000001, 0x6e69616d, 0x00000000, 0x00060010, 
    0x00000001, 0x00000011, 0x00000001, 0x00000001, 0x00000001, 0x00020013, 0x00000002, 0x00030016, 
    0x00000003, 0x00000020, 0x00040015, 0x00000004, 0x00000020, 0x00000001, 0x0004002b, 0x00000004, 
    0x00000005, 0x00000004, 0x0004001c, 0x00000006, 0x00000003, 0x00000005, 0x0004001e, 0x00000007, 
    0x00000003, 0x00000006, 0x00050048, 0x00000007, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 
    0x00000007, 0x00000001, 0x00000023, 0x00000010, 0x00030047, 0x00000007, 0x00000002, 0x00040020, 
    0x00000008, 0x00000002, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000002, 0x00040047, 
    0x00000009, 0x00000021, 0x00000000, 0x00040047, 0x00000009, 0x00000022, 0x00000000, 0x00030021, 
    0x0000000a, 0x00000003, 0x00050036, 0x00000003, 0x00000001, 0x00000000, 0x0000000a, 0x000200f8, 
    0x0000000b, 0x0004002b, 0x00000003, 0x0000000c, 0x41200000, 0x0004002b, 0x00000003, 0x0000000d, 
    0x40000000, 0x00050081, 0x00000003, 0x0000000e, 0x0000000c, 0x0000000d, 0x00050088, 0x00000003, 
    0x0000000f, 0x0000000e, 0x0000000d, 0x000200fe, 0x0000000f, 0x00010038
};
    jit_compile_spirv(code, sizeof(code)/4);
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

    /* Initialize MSI */
    msi_init(pdev, 0, 1, true, false, errp);

    /* Initialize Threading */
    gpu->threads_exit = false;
    
    qemu_mutex_init(&gpu->cmd_mutex);
    qemu_cond_init(&gpu->cmd_cond);
    gpu->cmd_pending = false;
    qemu_thread_create(&gpu->cmd_thread, "gpu-cmd-thread", gpu_cmd_thread_worker, gpu, QEMU_THREAD_JOINABLE);

    qemu_mutex_init(&gpu->dma_mutex);
    qemu_cond_init(&gpu->dma_cond);
    gpu->dma_pending = false;
    qemu_thread_create(&gpu->dma_thread, "gpu-dma-thread", gpu_dma_thread_worker, gpu, QEMU_THREAD_JOINABLE);

    qemu_mutex_init(&gpu->render_mutex);

    // Allocate internal shadow buffer (fixed size for simplicity)
    gpu->internal_fb = g_malloc0(1024 * 1024 * 4); 
    qemu_thread_create(&gpu->refresh_thread, "gpu-refresh-thread", gpu_refresh_thread_worker, gpu, QEMU_THREAD_JOINABLE);

    init_thread_pool();
}

/* Uninitialize GPU device */
static void pci_gpu_uninit(PCIDevice *pdev)
{
    GpuState *gpu = GPU(pdev);
    
    gpu->threads_exit = true;

    qemu_mutex_lock(&gpu->cmd_mutex);
    qemu_cond_signal(&gpu->cmd_cond);
    qemu_mutex_unlock(&gpu->cmd_mutex);

    qemu_mutex_lock(&gpu->dma_mutex);
    qemu_cond_signal(&gpu->dma_cond);
    qemu_mutex_unlock(&gpu->dma_mutex);

    qemu_thread_join(&gpu->cmd_thread);
    qemu_thread_join(&gpu->dma_thread);
    qemu_thread_join(&gpu->refresh_thread);

    qemu_mutex_destroy(&gpu->cmd_mutex);
    qemu_cond_destroy(&gpu->cmd_cond);
    qemu_mutex_destroy(&gpu->dma_mutex);
    qemu_cond_destroy(&gpu->dma_cond);
    qemu_mutex_destroy(&gpu->render_mutex);

    g_free(gpu->internal_fb);

    msi_uninit(pdev);
}
