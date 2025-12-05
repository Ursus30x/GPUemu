//#define DEBUG_P_REG

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
static void gpu_print_cmd(void *opaque)
{
    GpuState *s = opaque;
    printf("GPU Registers:\n");
    printf("  GPU_MODE            = %X\n",  REG_GPU_MODE(s));
    printf("  PROJECTION_MATRIX   = %X\n",  REG_VERTEX_SHADER(s));
    printf("  UPDATE_RENDER       = %X\n",  REG_UPDATE_RENDER(s));
    printf("  UPDATE_FB           = %X\n",  REG_UPDATE_FB(s));
    printf("  FB_WIDTH            = %X\n",  REG_FB_WIDTH(s));
    printf("  FB_HEIGHT           = %X\n",  REG_FB_HEIGHT(s));
    printf("  VERTEX_SIZE         = %X\n",  REG_VERTEX_SIZE(s));
    printf("  EDGE_SIZE           = %X\n",  REG_EDGE_SIZE(s));
}


static uint64_t lower_n_bytes(uint64_t data, unsigned nbytes) 
{
	uint64_t result;

	if (nbytes < 8) {
		uint64_t bitcount = ((uint64_t)nbytes)<<3;
		uint64_t mask = (1ULL << bitcount)-1;

		result = data & mask;
	} else {
		result = data;
	}
    
	return result;
}

static void simple_3d_mode(GpuState *gpu)
{

    if(REG_GPU_MODE(gpu) == GPU_MODE_3D)
    {
        exec_shader(gpu, REG_VERTEX_SHADER(gpu));

        vga_update_display(gpu);
        graphic_hw_update(gpu->con);
    }
}
/* cmd callbacks */
static uint64_t gpu_cmd_read(void *opaque, hwaddr addr, unsigned size)
{
    GpuState *gpu = opaque;

	uint64_t index = lower_n_bytes(addr, size);
	uint32_t index_u32 = index / sizeof(uint32_t);
	uint64_t result = ((uint32_t*)gpu->cmd)[index_u32];

	printf("reading idx %lu = %lu\n", index, result);

	return result;
}

static void gpu_cmd_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    if(addr > GPU_CMD_SIZE)
    {
        printf("Invalid address BAR0!\n");
        return;
    }

    GpuState *s = opaque;
   
    s->cmd[addr] = val;

    if(addr == REG_UPDATE_RENDER_ADDR)
    {
        // trigger update renderer
    }
    if(addr == REG_UPDATE_FB_ADDR)
    {
        // trigger update framebuffer
    }

    printf("cmd Write: addr=0x%llx val=0x%llx size=%u\n",
           (unsigned long long)addr, (unsigned long long)val, size);
    gpu_print_cmd(opaque);
}

static const MemoryRegionOps gpu_cmd_ops = {
    .read = gpu_cmd_read,
    .write = gpu_cmd_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};



static void pci_gpu_register_types(void)
{
    static InterfaceInfo interfaces[] = {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
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
	GpuState* gpu = opaque;
    if(REG_GPU_MODE(gpu) == GPU_MODE_3D)
        gpu_render_frame(opaque);

    uint32_t width =  REG_FB_WIDTH(gpu);
    uint32_t height =  REG_FB_HEIGHT(gpu);
    DisplaySurface *surface = qemu_console_surface(gpu->con);
	for(uint32_t i = 0; i<width*height; i++) 
		((uint32_t*)surface_data(surface))[i] = FB(gpu)[i];
    dpy_gfx_update(gpu->con, 0, 0, 640, 480);

}

static const GraphicHwOps ghwops = {
   .invalidate  = vga_invalidate_display,
   .gfx_update  = vga_update_display,
   .text_update = vga_update_text,
};

static void gpu_class_init(ObjectClass *class, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

    k->realize = pci_gpu_realize;
    k->exit    = pci_gpu_uninit;
    k->vendor_id = PCI_VENDOR_ID_CUSTOM;
    k->device_id = GPU_DEVICE_ID;
    k->revision  = 0x01;
    k->class_id  = PCI_CLASS_DISPLAY_OTHER;
}

static void timer_callback(void *opaque)
{

    GpuState *gpu = opaque;
    simple_3d_mode(gpu);
    /* Re-arm the periodic timer */
    timer_mod(gpu->timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 10000000ULL);
}

/* Realize GPU device */
static void pci_gpu_realize(PCIDevice *pdev, Error **errp)
{
    printf("pci_gpu_realize\n");

    GpuState *gpu = GPU(pdev);

    /* BAR0: cmd registers */
    memory_region_init_io(&gpu->cmdmem, OBJECT(gpu), &gpu_cmd_ops, gpu, "gpu-cmd", GPU_CMD_SIZE);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &gpu->cmdmem);

    /* BAR1:  VRAM */
    memory_region_init_ram(&gpu->vrammem, OBJECT(gpu), "gpu-vram", GPU_VRAM_SIZE, errp);
    pci_register_bar(pdev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &gpu->vrammem);

    gpu->vram_ptr = memory_region_get_ram_ptr(&gpu->vrammem);

    gpu->con = graphic_console_init(DEVICE(pdev), 0, &ghwops, gpu);
    //init state
    REG_FB_HEIGHT(gpu) = 480;
    REG_FB_WIDTH(gpu) = 640;
    REG_VERTEX_SIZE(gpu) = 0;
    REG_EDGE_SIZE(gpu) = 0;
    REG_VERTEX_SHADER(gpu) = 0;
    REG_GPU_MODE(gpu) = GPU_MODE_GOP;
 
    REG_EXEC_VERTEX_SHADER(gpu) = 1;
    REG_EXEC_FRAGMENT_SHADER(gpu) = 1;
    
    REG_FRAGMENT_SHADER(gpu) = 300;

    void *ss = SHADER_PROGRAM(gpu) + REG_VERTEX_SHADER(gpu);
    /*
    addf p2 p2 0.02
    fsan p2
    roty m0 p2
    rotx m1 0.3232
    mulm m2 m0 m1
    trans m1 0 0 5
    mulm m0 m1 m2
    mvp m0
    exit
    */
    uint64_t bin_vertex_shader[] = { 0x241020909, 0x3CA3D70A, 0x4002090E, 0x0, 0x281000903, 0x0, 0x3EA57A7880010902, 0x0, 0x85020901, 0x1, 0x80010905, 0x500000000, 0x185000901, 0x2, 0x80000906, 0x0, 0x907, 0x0 };
    memcpy(ss, bin_vertex_shader, sizeof(bin_vertex_shader));
    /*
    mov pr 255
    mov pg 0
    mov pb 0
    cmpi gt px 300
    !mov pb 100
    exit
    */
    uint64_t bin_fragment_shader[] ={ 0xFF000A0900, 0x0, 0xB0900, 0x0, 0xC0900, 0x0, 0x801000408, 0x12C, 0x64000C0800, 0x0, 0xA0800, 0x0, 0x907, 0x0 };

    memcpy(ss + REG_FRAGMENT_SHADER(gpu) , bin_fragment_shader, sizeof(bin_fragment_shader));

    gpu->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, timer_callback, gpu);
    timer_mod(   gpu->timer , qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 100000000ULL);
}

/* Uninitialize GPU device */
static void pci_gpu_uninit(PCIDevice *pdev)
{
}
