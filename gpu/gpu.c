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
typedef struct { double x, y, z; uint32_t rgba; } Vec3;
typedef struct { uint32_t a, b; } Edge;
typedef struct { double m[4][4]; }  Mat4;
typedef struct { double x, y, z, w; } Vec4;

#define TYPE_PCI_GPU_DEVICE "AREK"
#define GPU_DEVICE_ID 0x2137
#define PCI_VENDOR_ID_CUSTOM 0x6969
#define GPU_FB_WIDTH 640
#define GPU_FB_HEIGHT 480

#define PI 3.14159265358979323846

#define GPU_VRAM_SIZE (1 << 25)  // 32 MB
#define GPU_CMD_SIZE 0x1000 

#define REG_MAT_SIZE 8
#define REG_NUM_OK(num) (num < REG_MAT_SIZE)
#define REG_NUM(r)        ((r) & 0x7FFFFFFF)

#define REG_GPU_MODE_ADDR 0
#define REG_EXEC_SHADER_ADDR 1 
#define REG_UPDATE_RENDER_ADDR 2
#define REG_UPDATE_FB_ADDR 3
#define REG_FB_WIDTH_ADDR 4
#define REG_FB_HEIGHT_ADDR 8
#define REG_VERTEX_SIZE_ADDR 12 
#define REG_EDGE_SIZE_ADDR 16
#define REG_SHADER_ADDR 20

#define REG_GPU_MODE(s) s->cmd[REG_GPU_MODE_ADDR]
#define REG_EXEC_SHADER(s) s->cmd[REG_EXEC_SHADER_ADDR]
#define REG_UPDATE_RENDER(s) s->cmd[REG_UPDATE_RENDER_ADDR]
#define REG_UPDATE_FB(s) s->cmd[REG_UPDATE_FB_ADDR]
#define REG_FB_WIDTH(s) *(uint32_t*)&s->cmd[REG_FB_WIDTH_ADDR]
#define REG_FB_HEIGHT(s) *(uint32_t*)&s->cmd[REG_FB_HEIGHT_ADDR]
#define REG_VERTEX_SIZE(s) *(uint32_t*)&s->cmd[REG_VERTEX_SIZE_ADDR]
#define REG_EDGE_SIZE(s) *(uint32_t*)&s->cmd[REG_EDGE_SIZE_ADDR]
#define REG_SHADER(s) *(uint32_t*)&s->cmd[REG_SHADER_ADDR]

/*
Divide VRAM into segments:
  - FB SEGMENT     8  MB offset: 0x0000000
  - VERTEX SEGMENT 8  MB offset: 0x0800000
  - EDGES SEGMENT  8  MB offset: 0x1000000
  - SHADER SEGMENT 8  MB offset: 0x1800000
*/
#define GPU_VRAM_FB_SEGMENT_ADDR      0x0000000
#define GPU_VRAM_VERTEX_SEGMENT_ADDR  0x0800000
#define GPU_VRAM_EDGES_SEGMENT_ADDR   0x1000000
#define GPU_VRAM_SHATER_SEGMENT_ADDR  0x1800000

#define GPU_VRAM_FB_SEGMENT(s)      &((s)->vram_ptr[GPU_VRAM_FB_SEGMENT_ADDR])
#define GPU_VRAM_VERTEX_SEGMENT(s)  &((s)->vram_ptr[GPU_VRAM_VERTEX_SEGMENT_ADDR])
#define GPU_VRAM_EDGES_SEGMENT(s)   &((s)->vram_ptr[GPU_VRAM_EDGES_SEGMENT_ADDR])
#define GPU_VRAM_SHATER_SEGMENT(s)  &((s)->vram_ptr[GPU_VRAM_SHATER_SEGMENT_ADDR])

#define FB(s)            ((uint32_t*) GPU_VRAM_FB_SEGMENT(s))
#define VERTEX_TABLE(s)  ((Vec3*)    GPU_VRAM_VERTEX_SEGMENT(s))
#define EDGES_TABLE(s)   ((Edge*)    GPU_VRAM_EDGES_SEGMENT(s))
#define SHADER_PROGRAM(s)((void*)    GPU_VRAM_SHATER_SEGMENT(s))

typedef struct GpuState {
    PCIDevice pdev;
    MemoryRegion cmdmem;   // BAR0 commannds
    MemoryRegion vrammem;  // BAR1 VRAM
    QemuConsole *con;
    QEMUTimer *timer;

    uint8_t  cmd[GPU_CMD_SIZE];
    uint8_t  *vram_ptr;

    Mat4 regs[REG_MAT_SIZE];
    Mat4 mvp;
} GpuState;


typedef union {
    uint32_t u32;
    float    f32;
} InstrArg;

typedef struct Instr {
    uint8_t  opcode;
    uint8_t  dst;
    uint16_t opt;
    InstrArg arg0;
    InstrArg arg1;
    InstrArg arg2;
} Instr;



#define REG_MN(n) (1 << 31) | n
#define REG_M0 REG_MN(0)
#define REG_M1 REG_MN(1)
#define REG_M2 REG_MN(2)
#define REG_M3 REG_MN(3)
#define REG_M4 REG_MN(4)
#define REG_M5 REG_MN(5)
#define REG_M6 REG_MN(6)
#define REG_M7 REG_MN(7)


#define ARG_IS_MEM_ADDR(arg) ((arg >> 31) == 0) 
/*
ISA
0 MOV dst src
1 MUL dst src0 src1
2 ROTX dst src0
3 ROTY dst src0
4 IDENT dst
5 TRANS dst src0 src1 src2
6 MVP dst
7 EXIT
*/
#define INSTR_MOV 0
#define INSTR_MUL 1
#define INSTR_ROTX 2
#define INSTR_ROTY 3
#define INSTR_IDENT 4
#define INSTR_TRANS 5
#define INSTR_MVP 6
#define INSTR_EXIT 7



#define ARG_U32(x) ((InstrArg){ .u32 = (uint32_t)(uintptr_t)(x) })
#define ARG_F32(x) ((InstrArg){ .f32 = (float)(x) })

#define MAKE_INSTR(op, dst, a0, a1, a2) \
    (Instr){ (uint8_t)(op), (uint8_t)(dst), 0, (a0), (a1), (a2) }

#define I_MOV(dst, src)              MAKE_INSTR(INSTR_MOV,   dst, ARG_U32(src), ARG_U32(0),      ARG_U32(0))
#define I_MUL(dst, a0, a1)           MAKE_INSTR(INSTR_MUL,   dst, ARG_U32(a0), ARG_U32(a1),     ARG_U32(0))
#define I_ROTX(dst, a0)              MAKE_INSTR(INSTR_ROTX,  dst, ARG_F32(a0), ARG_U32(0),      ARG_U32(0))
#define I_ROTY(dst, a0)              MAKE_INSTR(INSTR_ROTY,  dst, ARG_F32(a0), ARG_U32(0),      ARG_U32(0))
#define I_IDENT(dst)                 MAKE_INSTR(INSTR_IDENT, dst, ARG_U32(0),  ARG_U32(0),      ARG_U32(0))
#define I_TRANS(dst, a0, a1, a2)     MAKE_INSTR(INSTR_TRANS, dst, ARG_F32(a0), ARG_F32(a1),     ARG_F32(a2))
#define I_MVP(dst)                   MAKE_INSTR(INSTR_MVP,   dst, ARG_U32(0),  ARG_U32(0),      ARG_U32(0))
#define I_EXIT()                     MAKE_INSTR(INSTR_EXIT,  0,   ARG_U32(0),  ARG_U32(0),      ARG_U32(0))

#define INSTR_TABLE(name, ...) \
    Instr name[] = { __VA_ARGS__ };

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
    printf("  PROJECTION_MATRIX   = %X\n",  REG_EXEC_SHADER(s));
    printf("  UPDATE_RENDER       = %X\n",  REG_UPDATE_RENDER(s));
    printf("  UPDATE_FB           = %X\n",  REG_UPDATE_FB(s));

    printf("  FB_WIDTH            = %X\n",  REG_FB_WIDTH(s));
    printf("  FB_HEIGHT           = %X\n",  REG_FB_HEIGHT(s));
    printf("  VERTEX_SIZE         = %X\n",  REG_VERTEX_SIZE(s));
    printf("  EDGE_SIZE           = %X\n",  REG_EDGE_SIZE(s));
}

static void gpu_render_frame(void *opaque);
/* cmd callbacks */
static uint64_t gpu_cmd_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("cmd Read: addr=0x%llx size=%u\n", (unsigned long long)addr, size);
    return 0;
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



static inline void put_pixel(GpuState *gpu, int x, int y, uint32_t color)
{
    if (x < 0 || x >= GPU_FB_WIDTH || y < 0 || y >= GPU_FB_HEIGHT)
        return;
   FB(gpu)[y * GPU_FB_WIDTH + x] = color;
}



static void draw_line(GpuState *gpu, int x0, int y0, int x1, int y1, uint32_t color1, uint32_t color2) 
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int length = (int)sqrtf((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0));
    if (length == 0) length = 1;
    int step = 0;

    for (;;) 
    {
        float t = (float)step / length;

        uint8_t a1 = (color1 >> 24) & 0xFF;
        uint8_t r1 = (color1 >> 16) & 0xFF;
        uint8_t g1 = (color1 >> 8) & 0xFF;
        uint8_t b1 = color1 & 0xFF;

        uint8_t a2 = (color2 >> 24) & 0xFF;
        uint8_t r2 = (color2 >> 16) & 0xFF;
        uint8_t g2 = (color2 >> 8) & 0xFF;
        uint8_t b2 = color2 & 0xFF;

        uint32_t color = ((uint32_t)(a1 + t*(a2 - a1)) << 24) |
                         ((uint32_t)(r1 + t*(r2 - r1)) << 16) |
                         ((uint32_t)(g1 + t*(g2 - g1)) << 8) |
                         ((uint32_t)(b1 + t*(b2 - b1)));

        put_pixel(gpu, x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        step++;
    }
}


static Vec4 mat4_mul_vec4(Mat4 *mat, Vec4 v)
 {
    Vec4 r;
    r.x = mat->m[0][0]*v.x + mat->m[0][1]*v.y + mat->m[0][2]*v.z + mat->m[0][3]*v.w;
    r.y = mat->m[1][0]*v.x + mat->m[1][1]*v.y + mat->m[1][2]*v.z + mat->m[1][3]*v.w;
    r.z = mat->m[2][0]*v.x + mat->m[2][1]*v.y + mat->m[2][2]*v.z + mat->m[2][3]*v.w;
    r.w = mat->m[3][0]*v.x + mat->m[3][1]*v.y + mat->m[3][2]*v.z + mat->m[3][3]*v.w;
    return r;
}

static Mat4 mat4_mul(Mat4 *a, Mat4 *b) 
{
    Mat4 r = {0};
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            for(int k=0;k<4;k++)
                r.m[i][j] += a->m[i][k] * b->m[k][j];
    return r;
}

static Mat4 mat4_identity(void) 
{
    Mat4 m = {0};
    for(int i=0;i<4;i++) m.m[i][i] = 1.0f;
    return m;
}

static Mat4 mat4_rotate_y(float angle) 
{
    Mat4 m = mat4_identity();
    m.m[0][0] = cosf(angle);  m.m[0][2] = sinf(angle);
    m.m[2][0] = -sinf(angle); m.m[2][2] = cosf(angle);
    return m;
}

static Mat4 mat4_rotate_x(float angle) 
{
    Mat4 m = mat4_identity();
    m.m[1][1] = cosf(angle);  m.m[1][2] = -sinf(angle);
    m.m[2][1] = sinf(angle);  m.m[2][2] = cosf(angle);
    return m;
}

static Mat4 mat4_translate(float x, float y, float z) 
{
    Mat4 m = mat4_identity();
    m.m[0][3] = x; m.m[1][3] = y; m.m[2][3] = z;
    return m;
}

static Mat4 mat4_perspective(float fov, float aspect, float near, float far) 
{
    Mat4 m = {0};
    float f = 1.0f / tanf(fov * 0.5f);
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = far / (far - near);
    m.m[2][3] = (-far * near) / (far - near);
    m.m[3][2] = 1.0f;
    return m;
}
static float angle = PI / 3;


static inline Mat4 get_mat_from_arg(int arg_val, Mat4* gpu_regs, uint8_t* shader_segment) 
{
    Mat4 mat;
    if (ARG_IS_MEM_ADDR(arg_val)) {
        memcpy(&mat, shader_segment + arg_val, sizeof(Mat4));
    } else {
        uint32_t src = REG_NUM(arg_val);

        mat = gpu_regs[src];
    }

    return mat;
}
static inline void print_mat4(const Mat4* mat, const char* name)
{
    if (!mat) return;
    printf("Matrix %s:\n", name);
    for (int i = 0; i < 4; ++i) {
        printf("[ ");
        for (int j = 0; j < 4; ++j) {
            printf("%8.3f ", mat->m[i][j]);
        }
        printf("]\n");
    }
    printf("\n");
}
static void exec_shader(GpuState *gpu)
{
    uint32_t program_offset = REG_SHADER(gpu);
    void *shader_segment = SHADER_PROGRAM(gpu);
    void *program_address = shader_segment + program_offset;
    printf("program_offset: %X\n", program_offset);
    printf("shader_segment: %p\n", shader_segment);
    printf("program_address: %p\n", program_address);
    int end = 1;
    #define CHECK_REG_NUM(num) if(!REG_NUM_OK(num)){printf("Panic invalid regnum!\n");end = 0;break;}

    do{
        printf("read\n");
        Instr instr = *(Instr*)program_address;
        printf("OPCODE: %d\n", instr.opcode);
        switch (instr.opcode)
        {
        case INSTR_MOV:
        {
            CHECK_REG_NUM(instr.dst);
            if(ARG_IS_MEM_ADDR(instr.arg0.u32))
                memcpy(&gpu->regs[instr.dst], shader_segment+instr.arg0.u32,  sizeof(Mat4));
            else
            {
                uint32_t src = REG_NUM(instr.arg0.u32);
                CHECK_REG_NUM(src);
                gpu->regs[instr.dst] = gpu->regs[src];
            }
            break;
        }
        case INSTR_MUL:
        {
            CHECK_REG_NUM(instr.dst);

            Mat4 a = get_mat_from_arg(instr.arg0.u32, gpu->regs, shader_segment);
            Mat4 b = get_mat_from_arg(instr.arg1.u32, gpu->regs, shader_segment);
            gpu->regs[instr.dst] = mat4_mul(&a,&b);
            print_mat4(&gpu->regs[instr.dst], "mul");
            break;
        }
        case INSTR_MVP:
            CHECK_REG_NUM(instr.dst);
            gpu->mvp = gpu->regs[instr.dst];
            print_mat4(&gpu->mvp, "mvp");
            break;
        case INSTR_ROTX:
            CHECK_REG_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_rotate_x(instr.arg0.f32);
            print_mat4(&gpu->regs[instr.dst], "rotx");
            break;
        case INSTR_ROTY:
            CHECK_REG_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_rotate_y(instr.arg0.f32);
            print_mat4(&gpu->regs[instr.dst], "roty");
            break;
        case INSTR_TRANS:
            CHECK_REG_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_translate(instr.arg0.f32,instr.arg1.f32,instr.arg2.f32);
            print_mat4(&gpu->regs[instr.dst], "trans");
            break;
        case INSTR_IDENT:
            CHECK_REG_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_identity();
            break;
        case INSTR_EXIT:
            end = 0;
            REG_EXEC_SHADER(gpu) = 0;
            printf("[gpu] Exit shader\n");
            break;
        default:
            printf("Panic unkown opcode!\n");
            end = 0;
            break;
        }
        program_address+=sizeof(Instr);
    }while(end);

}



static void gpu_render_frame(void *opaque)
{
    GpuState *gpu = opaque;
    if(REG_EXEC_SHADER(gpu) == 1)
    {
        return;
    }

    uint32_t width =  REG_FB_WIDTH(gpu);
    uint32_t height =  REG_FB_HEIGHT(gpu);
    uint32_t vertex_size = REG_VERTEX_SIZE(gpu);
    uint32_t edges_size = REG_EDGE_SIZE(gpu);

    uint32_t *fb = FB(gpu);
    Vec3 *vertices = VERTEX_TABLE(gpu);
    Edge *edges = EDGES_TABLE(gpu);

    for(uint32_t i=0;i<width*height;i++) fb[i] = 0xFF000000;


    Mat4  ry        = mat4_rotate_y(0.2f);
    Mat4  rx        = mat4_rotate_x(0.2f);
    Mat4  model     = mat4_mul(&ry,&rx);
    Mat4  translate = mat4_translate(0, 0, 5);
    model           = mat4_mul(&translate, &model);

    Mat4  proj      = mat4_perspective(PI/3, (float)width/height, 1.0f, 10.0f);
    Mat4  mvp_local       = mat4_mul(&proj, &gpu->mvp);
    //

    uint32_t *px = malloc(sizeof(uint32_t)* vertex_size);
    uint32_t *py = malloc(sizeof(uint32_t)* vertex_size);
    for(uint32_t i=0;i<vertex_size;i++) 
    {
        Vec4 v = {vertices[i].x, vertices[i].y, vertices[i].z, 1.0f};
        Vec4 tv = mat4_mul_vec4(&mvp_local, v);

        float ndc_x = tv.x / tv.w;
        float ndc_y = tv.y / tv.w;
        px[i] = (int)((ndc_x*0.5f + 0.5f) * width);
        py[i] = (int)((-ndc_y*0.5f + 0.5f) * height);
    }

    for(uint32_t i=0;i<edges_size;i++)
    {
        Edge e = edges[i];
        draw_line(gpu, px[e.a], py[e.a], px[e.b], py[e.b],  vertices[e.a].rgba, vertices[e.b].rgba);
    }
    free(px);
    free(py);
}

static void vga_update_display(void *opaque)
{
	GpuState* gpu = opaque;
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
    k->class_id  = PCI_CLASS_OTHERS;
}

static void timer_callback(void *opaque)
{
    GpuState *gpu = opaque;

    angle+=0.002f;
    /*
    rotx m0, angle
    roty m1, angle
    mul m2, m0, m1
    trans m1, 0, 0, 5
    mul m0, m1, m2
    mvp m0
    exit
    */
    void *ss = SHADER_PROGRAM(gpu) + REG_SHADER(gpu);
    INSTR_TABLE(program,
    I_ROTX(REG_M0, angle),          
    I_ROTY(REG_M1, angle),           
    I_MUL(REG_M2, REG_M0, REG_M1), 
    I_TRANS(REG_M1, 0, 0, 5), 
    I_MUL(REG_M0, REG_M1, REG_M2), 
    I_MVP(REG_M0),
    I_EXIT()
    );
    memcpy(ss, program, sizeof(program));
    REG_EXEC_SHADER(gpu) = 1;
    exec_shader(gpu);

    vga_update_display(gpu);

    graphic_hw_update(gpu->con);
    /* Re-arm the periodic timer */
    timer_mod(gpu->timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 1000000ULL);
}
// static const char* st(uint8_t op) {
//     switch(op) {
//         case INSTR_MOV:   return "MOV";
//         case INSTR_MUL:   return "MUL";
//         case INSTR_ROTX:  return "ROTX";
//         case INSTR_TRANS: return "TRANS";
//         case INSTR_EXIT:  return "EXIT";
//         default:          return "UNKNOWN";
//     }
// }

// static void print_program(Instr* prog, size_t count) {
//     for(size_t i = 0; i < count; i++) {
//         Instr* instr = &prog[i];
//         printf("%02zu: %s dst=%u arg0=%u arg1=%u arg2=%u\n",
//                i, st(instr->opcode),
//                instr->dst,
//                instr->arg0.u32,
//                instr->arg1.u32,
//                instr->arg2.u32);
//     }
//}


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
    REG_SHADER(gpu) = 0;
 
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
    Edge cube_edges[] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7},{5,3}
    };

    Vec3 *vertices = VERTEX_TABLE(gpu);
    Edge *edges = EDGES_TABLE(gpu);

    memcpy(vertices, cube_vertices, sizeof(cube_vertices));
    memcpy(edges, cube_edges, sizeof(cube_edges));

        REG_EXEC_SHADER(gpu) = 1;

    gpu->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, timer_callback, gpu);
    timer_mod(   gpu->timer , qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 100000000ULL);
}

/* Uninitialize GPU device */
static void pci_gpu_uninit(PCIDevice *pdev)
{
}
