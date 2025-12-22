// #define DEBUG_P_REG

#include "gpu.h"
#include "math3d.h"
#include "renderer.h"

DECLARE_INSTANCE_CHECKER(GpuState, GPU, TYPE_PCI_GPU_DEVICE);

static void pci_gpu_register_types(void);
static void gpu_instance_init(Object *obj);
static void gpu_class_init(ObjectClass *class, const void *data);
static void pci_gpu_realize(PCIDevice *pdev, Error **errp);
static void pci_gpu_uninit(PCIDevice *pdev);
static void vga_update_display(void *opaque);

type_init(pci_gpu_register_types)

    // static uint64_t lower_n_bytes(uint64_t data, unsigned nbytes)
    // {
    // 	uint64_t result;

    // 	if (nbytes < 8) {
    // 		uint64_t bitcount = ((uint64_t)nbytes)<<3;
    // 		uint64_t mask = (1ULL << bitcount)-1;

    // 		result = data & mask;
    // 	} else {
    // 		result = data;
    // 	}

    // 	return result;
    // }

static void simple_3d_mode(GpuState *gpu)
{

    if (gpu->gpu_mode == GPU_MODE_3D)
    {
        //exec_shader(gpu, gpu->vs_code_addr);
        gpu_render_frame(gpu);
        vga_update_display(gpu);
        graphic_hw_update(gpu->con);
    }
}

static void gpu_print_mmio(GpuState *s)
{
    printf("\n--- GPU MMIO Register Snapshot ---\n");
    printf("0x00 [GPU_MODE]:         0x%08x\n", s->gpu_mode);
    printf("0x04 [RING_HEAD]:        0x%08x\n", s->ring_buffer_head);
    printf("0x08 [RING_TAIL]:        0x%08x\n", s->ring_buffer_tail);
    printf("0x10 [VS_CODE_PTR]:      0x%08x\n", s->vs_code_addr);
    printf("0x14 [FS_CODE_PTR]:      0x%08x\n", s->fs_code_addr);
    printf("0x18 [WIDTH]:            %u\n", s->width);
    printf("0x1C [HEIGHT]:           %u\n", s->height);
    printf("0x20 [FB_ADDR]:          0x%08x\n", s->framebuffer_vram_offset);
    printf("0x24 [GPU_TIME]:         %u\n", s->gpu_time);
    printf("----------------------------------\n");
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
        printf("[CMD] Clear FB %x \n", (gpu->width * gpu->height));
        for (uint32_t i = 0; i < (gpu->width * gpu->height); i++)
            FB(gpu)[i] = 0xff000000;
        break;
    case CMD_SET_STATE:
        printf("[CMD] Set state\n");
        switch (cmd->payload.state.state_id)
        {
        case STATE_ID_EDGE_CONFIG:
            gpu->edge_config = cmd->payload.state.value.buffer_config;
            break;
        case STATE_ID_VBO_CONFIG:
            gpu->vbo_config = cmd->payload.state.value.buffer_config;
            break;
        case STATE_ID_UNIFORM_CONFIG:
            gpu->uinform_config = cmd->payload.state.value.buffer_config;
            break;
        case STATE_ID_SHADER_PTRS:
            gpu->fs_code_addr = cmd->payload.state.value.shader_ptrs.fs_addr;
            gpu->vs_code_addr = cmd->payload.state.value.shader_ptrs.vs_addr;
            break;
        default:
            break;
        }
        break;
    case CMD_DRAW_PRIMITIVE:
        if(cmd->payload.draw.type == PRIMITIVE_TYPE_LINES)
            simple_3d_mode(gpu);
        printf("[CMD] Draw primitive\n");
        break;
    case CMD_NOOP:
        printf("[CMD] NOP\n");
    default:
        break;
    }
}
static void process_ring_buffer(GpuState *s)
{

    uint32_t head = s->ring_buffer_head;
    uint32_t tail = s->ring_buffer_tail;

    if (head == tail)
    {
        printf("[GPU CMD PROC] Ring Buffer is empty (head == tail: 0x%x)\n", head);
        return;
    }

    printf("[GPU CMD PROC] Processing buffer: Head=0x%x, Tail=0x%x, Size=%lu\n",
           head, tail, (long)sizeof(Command));

    while (s->ring_buffer_tail != head)
    {

        uint8_t *cmd_ptr = s->vram_ptr + s->ring_buffer_tail;
        Command *cmd = (Command *)cmd_ptr;

        printf("[GPU CMD PROC] Executing command at VRAM Offset 0x%x (Opcode: 0x%x)\n",
               s->ring_buffer_tail, cmd->opcode);

        execute_command(s, cmd);

        s->ring_buffer_tail = (s->ring_buffer_tail + sizeof(Command));

        if (s->ring_buffer_tail == 0 && head != 0)
        {
            printf("[GPU CMD PROC] Ring Buffer wrapped around.\n");
        }
    }

    printf("[GPU CMD PROC] Finished processing. New Tail=0x%x. GPU Mode: 0x%x\n", s->ring_buffer_tail, s->gpu_mode);
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
    case 0x00:
        target_reg = &s->gpu_mode;
        break;
    case 0x04:
        target_reg = &s->ring_buffer_head;
        trigger_command_processor = 1;
        break;
    case 0x08:
        fprintf(stderr, "GPU MMIO WRITE: Warning: Attempted write to read-only RING_TAIL at 0x%" PRIx64 "\n", addr);
        return;
    case 0x10:
        target_reg = &s->vs_code_addr;
        break;
    case 0x14:
        target_reg = &s->fs_code_addr;
        break;
    case 0x18:
        target_reg = &s->width;
        break;
    case 0x1C:
        target_reg = &s->height;
        break;
    case 0x20:
        target_reg = &s->framebuffer_vram_offset;
        break;
    case 0x24:
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
        printf("GPU MMIO WRITE: 0x%lx written to 0x%" PRIx64 " (size %u). New reg value: 0x%x\n", val, addr, size, *target_reg);
        if (trigger_command_processor)
        {
            printf("\n[GPU TRIGGER] Detected write to RING_HEAD (0x04). Launching command processor...\n");
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
    case 0x00:
        reg_val = s->gpu_mode;
        break;
    case 0x04:
        reg_val = s->ring_buffer_head;
        break;
    case 0x08:
        reg_val = s->ring_buffer_tail;
        break;
    case 0x10:
        reg_val = s->vs_code_addr;
        break;
    case 0x14:
        reg_val = s->fs_code_addr;
        break;
    case 0x18:
        reg_val = s->width;
        break;
    case 0x1C:
        reg_val = s->height;
        break;
    case 0x20:
        reg_val = s->framebuffer_vram_offset;
        break;
    case 0x24:
        reg_val = s->gpu_time;
        break;
    default:
        fprintf(stderr, "GPU MMIO READ: Unhandled base offset 0x%" PRIx64 "\n", base_addr);
        return 0;
    }

    uint64_t result = (reg_val >> (offset_in_word * 8)) & ((1ULL << (size * 8)) - 1);

    printf("GPU MMIO READ: 0x%" PRIx64 " returned from offset 0x%" PRIx64 " (size %u)\n", result, addr, size);
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
    printf("invalidated display\n");
}
static void vga_update_text(void *opaque, console_ch_t *chardata)
{
    printf("updated text\n");
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

static float angle = 0;

static void timer_callback(void *opaque)
{

    GpuState *gpu = opaque;
    angle += 0.02f;
    Mat4  mvp1, mvp2;
    {
        Mat4  ry        = mat4_rotate_y(angle);
        Mat4  rx        = mat4_rotate_x(0.2);
        Mat4  scale     = mat4_scale_uniform(0.5);
        Mat4  model     = mat4_mul(&ry,&rx);
        model           = mat4_mul(&scale,&model);
        Mat4  translate = mat4_translate(0, 0, 5);
        model           = mat4_mul(&translate, &model);

        Mat4  proj      = mat4_perspective(PI/3, (float)640/480, 1.0f, 10.0f);
        mvp1 = mat4_mul(&proj, &model);
    }
    memcpy(gpu->vram_ptr + 0x19b000, &mvp1, sizeof(mvp1));

    {
        Mat4  ry        = mat4_rotate_y(angle*2);
        Mat4  rx        = mat4_rotate_x(0);
        Mat4  scale     = mat4_scale_uniform(0.25);
        Mat4  model     = mat4_mul(&ry,&rx);
        model           = mat4_mul(&scale,&model);
        Mat4  translate = mat4_translate(0, 1, 5);
        model           = mat4_mul(&translate, &model);

        Mat4  proj      = mat4_perspective(PI/3, (float)640/480, 1.0f, 10.0f);
        mvp2 = mat4_mul(&proj, &model);
    }
    memcpy(gpu->vram_ptr + 0x21b000, &mvp2, sizeof(mvp2));

    uint32_t offset = 0x14b000;
    uint8_t *ring_buffer_base = gpu->vram_ptr + offset;

    GenericBufferConfig vbo_conf  = {.element_type = D_TYPE_VEC3, .size = 8, .addr = 0x15b000};
    GenericBufferConfig edge_conf = {.element_type = D_TYPE_VEC2, .size = 13, .addr = 0x16b000};
    GenericBufferConfig uniform   = {.element_type = D_TYPE_MAT4, .size = sizeof(Mat4), .addr = 0x19b000};
    GenericBufferConfig uniform2   = {.element_type = D_TYPE_MAT4, .size = sizeof(Mat4), .addr = 0x21b000};

    CMD_BEGIN();
    CMD_CLEAR_FB(ring_buffer_base);
    CMD_SET_VBO(ring_buffer_base, vbo_conf);
    CMD_SET_EDGE(ring_buffer_base, edge_conf);
    CMD_SET_UBO(ring_buffer_base, uniform);
    CMD_SET_SHADERS(ring_buffer_base, 0x18b000, 0x19c000);
    CMD_DRAW_WIREFRAME(ring_buffer_base);
    CMD_SET_UBO(ring_buffer_base, uniform2);
    CMD_DRAW_WIREFRAME(ring_buffer_base);
    CMD_DBG_END(offset);

    gpu->gpu_mode = GPU_MODE_3D;
    process_ring_buffer(gpu);
    timer_mod(gpu->timer,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 10000000ULL);
}

/* Realize GPU device */
static void pci_gpu_realize(PCIDevice *pdev, Error **errp)
{
    printf("pci_gpu_realize\n");

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

    printf("[INIT] Ring Buffer starts at VRAM offset 0x%x\n", 0);



     Vec3 cube_vertices[] = {
        { -1, -1, -1, 0xFFFF0000 },
        {  1, -1, -1, 0xFF00FF00 },
        {  1,  1, -1, 0xFF0000FF },
        { -1,  1, -1, 0xFFFFFF00 }, 
        { -1, -1,  1, 0xFFFF00FF },
        {  1, -1,  1, 0xFF00FFFF },
        {  1,  1,  1, 0xFFFFFFFF }, 
        { -1,  1,  1, 0xFF808080 }  
    };
    memcpy(gpu->vram_ptr + 0x15b000, cube_vertices, sizeof(cube_vertices));
    Edge cube_edges[] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7},{5,3}
    };
    memcpy(gpu->vram_ptr + 0x16b000, cube_edges, sizeof(cube_edges));
    

    uint64_t bin_shader[] = { 0x80000916, 0x0, 0xC5010901, 0x8, 0x80010906, 0x0, 0x907, 0x0 };
    memcpy(gpu->vram_ptr + 0x18b000,bin_shader, sizeof(bin_shader));

    uint64_t bin_fragment_shader[] ={ 0xFF000A0900, 0x0, 0xB0900, 0x0, 0xC0900, 0x0, 0x801000408, 0x12C, 0x64000C0800, 0x0, 0xA0800, 0x0, 0x907, 0x0 };
    memcpy(gpu->vram_ptr + 0x19c000 , bin_fragment_shader, sizeof(bin_fragment_shader));
  

    gpu->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, timer_callback, gpu);
    timer_mod(gpu->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 100000000ULL);
}

/* Uninitialize GPU device */
static void pci_gpu_uninit(PCIDevice *pdev)
{
}
