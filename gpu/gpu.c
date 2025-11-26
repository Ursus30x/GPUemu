#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"
#include "hw/pci/msi.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "qapi/visitor.h"
#include "ui/console.h"
#include <math.h>

#define TYPE_PCI_GPU_DEVICE "AREK"
#define GPU_DEVICE_ID 0x2137
#define PCI_VENDOR_ID_CUSTOM 0x6969
#define GPU_FB_WIDTH 640
#define GPU_FB_HEIGHT 480

#define PI 3.14159265358979323846

#define GPU_VRAM_SIZE (1 << 26)  // 32 MB
#define GPU_CMD_SIZE 0x1000 

#define REG_GPU_MODE_ADDR 0
#define REG_PROJECTION_MATRIX_ADDR 1 
#define REG_UPDATE_RENDER_ADDR 2
#define REG_UPDATE_FB_ADDR 3
#define REG_FB_WIDTH_ADDR 4
#define REG_FB_HEIGHT_ADDR 8
#define REG_VERTEX_SIZE_ADDR 12 
#define REG_EDGE_SIZE_ADDR 16

#define REG_GPU_MODE(s) s->cmd[REG_GPU_MODE_ADDR]
#define REG_PROJECTION_MATRIX(s) s->cmd[REG_PROJECTION_MATRIX_ADDR]
#define REG_UPDATE_RENDER(s) s->cmd[REG_UPDATE_RENDER_ADDR]
#define REG_UPDATE_FB(s) s->cmd[REG_UPDATE_FB_ADDR]
#define REG_FB_WIDTH(s) *(uint32_t*)&s->cmd[REG_FB_WIDTH_ADDR]
#define REG_FB_HEIGHT(s) *(uint32_t*)&s->cmd[REG_FB_HEIGHT_ADDR]
#define REG_VERTEX_SIZE(s) *(uint32_t*)&s->cmd[REG_VERTEX_SIZE_ADDR]
#define REG_EDGE_SIZE(s) *(uint32_t*)&s->cmd[REG_EDGE_SIZE_ADDR]

/*
Divide VRAM into segments:
  - FB SEGMENT     16 MB offset: 0x0000000
  - VERTEX SEGMENT 8  MB offset: 0x1000000
  - EDGES SEGMENT  7  MB offset: 0x1800000
  - SHADER SEGMENT 1  MB offset: 0x1F00000
*/
#define GPU_VRAM_FB_SEGMENT_ADDR      0x0000000
#define GPU_VRAM_VERTEX_SEGMENT_ADDR  0x2000000
#define GPU_VRAM_EDGES_SEGMENT_ADDR   0x2800000
#define GPU_VRAM_SHATER_SEGMENT_ADDR  0x2F00000

#define GPU_VRAM_FB_SEGMENT(s)      &((s)->vram_ptr[GPU_VRAM_FB_SEGMENT_ADDR])
#define GPU_VRAM_VERTEX_SEGMENT(s)  &((s)->vram_ptr[GPU_VRAM_VERTEX_SEGMENT_ADDR])
#define GPU_VRAM_EDGES_SEGMENT(s)   &((s)->vram_ptr[GPU_VRAM_EDGES_SEGMENT_ADDR])
#define GPU_VRAM_SHATER_SEGMENT(s)  &((s)->vram_ptr[GPU_VRAM_SHATER_SEGMENT_ADDR])

#define FB(s)            ((uint32_t*) GPU_VRAM_FB_SEGMENT(s))
#define VERTEX_TABLE(s)  ((Vec3*)    GPU_VRAM_VERTEX_SEGMENT(s))
#define EDGES_TABLE(s)   ((Edge*)    GPU_VRAM_EDGES_SEGMENT(s))
#define SHATER_PROGRAM(s)((void*)    GPU_VRAM_SHATER_SEGMENT(s))

typedef struct GpuState {
    PCIDevice pdev;
    MemoryRegion cmdmem;   // BAR0 commannds
    MemoryRegion vrammem;  // BAR1 VRAM
    QemuConsole *con;
    QEMUTimer *timer;

    uint32_t  cmd[GPU_CMD_SIZE];
    uint32_t  *vram_ptr;
} GpuState;



typedef struct Instr {
    uint8_t  *opcode;
    uint8_t   dst;
    uint32_t  arg0;
    uint32_t  arg1;
    uint32_t  arg2;
} Instr;
/*
ISA
0 MOV dst src
1 MUL dst src0 src1
2 ROTX dst src0
3 ROTY dst src0
4 IDENT dst
5 TRANS dst src0 src1 src2
6 SEND dst
7 EXIT
*/
DECLARE_INSTANCE_CHECKER(GpuState, GPU, TYPE_PCI_GPU_DEVICE)

static void pci_gpu_register_types(void);
static void gpu_instance_init(Object *obj);
static void gpu_class_init(ObjectClass *class, const void *data);
static void pci_gpu_realize(PCIDevice *pdev, Error **errp);
static void pci_gpu_uninit(PCIDevice *pdev);

type_init(pci_gpu_register_types)
static void gpu_print_cmd(void *opaque)
{
    GpuState *s = opaque;
    printf("GPU Registers:\n");
    printf("  GPU_MODE            = %X\n",  REG_GPU_MODE(s));
    printf("  PROJECTION_MATRIX   = %X\n",  REG_PROJECTION_MATRIX(s));
    printf("  UPDATE_RENDER       = %X\n",  REG_UPDATE_RENDER(s));
    printf("  UPDATE_FB           = %X\n",  REG_UPDATE_FB(s));

    printf("  FB_WIDTH            = %X\n",  REG_FB_WIDTH(s));
    printf("  FB_HEIGHT           = %X\n",  REG_FB_HEIGHT(s));
    printf("  VERTEX_SIZE         = %X\n",  REG_VERTEX_SIZE(s));
    printf("  EDGE_SIZE           = %X\n",  REG_EDGE_SIZE(s));
}

static uint64_t lower_n_bytes(uint64_t data, unsigned nbytes) {
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
    k->class_id  = PCI_CLASS_OTHERS;
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
    // gpu_render_frame((void *)s);

    //init state
    REG_FB_HEIGHT(gpu) = 480;
    REG_FB_WIDTH(gpu) = 640;
    REG_VERTEX_SIZE(gpu) = 8;
    REG_EDGE_SIZE(gpu) = 13;

}

/* Uninitialize GPU device */
static void pci_gpu_uninit(PCIDevice *pdev)
{
}
