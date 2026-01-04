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
#include <stdint.h>
#include <math.h>
#include "gpu_isa.h"
#include "gpu_hw.h"
#include "vram.h"

#ifndef GPU_H
#define GPU_H


#define GPU_VRAM_FB_SEGMENT(s)      (&((s)->vram_ptr[GPU_VRAM_FB_SEGMENT_ADDR]))
#define GPU_VRAM_VERTEX_SEGMENT(s)  (&((s)->vram_ptr[GPU_VRAM_VERTEX_SEGMENT_ADDR]))
#define GPU_VRAM_EDGES_SEGMENT(s)   (&((s)->vram_ptr[GPU_VRAM_EDGES_SEGMENT_ADDR]))
#define GPU_VRAM_SHADER_SEGMENT(s)  (&((s)->vram_ptr[GPU_VRAM_SHADER_SEGMENT_ADDR]))

#define FB(s)              ((uint32_t*) (gpu->vram_ptr + gpu->framebuffer_vram_offset))
#define VERTEX_TABLE(s)    ((Vec3*)     (gpu->vram_ptr + gpu->vbo_config.addr))
#define EDGES_TABLE(s)     ((Edge*)     (gpu->vram_ptr + gpu->edge_config.addr))
#define TRIANGLES_TABLE(s) ((Triangle*) (gpu->vram_ptr + gpu->edge_config.addr))
#define SHADER_PROGRAM(s)  ((void*)     GPU_VRAM_SHADER_SEGMENT(s))
#define Z_BUFFER(s)        ((float*)     (s->vram_ptr + s->zbuffer_addr))

#define REG_GPU_MODE(s)             (s->cmd[REG_GPU_MODE_ADDR])
#define REG_EXEC_VERTEX_SHADER(s)   (s->cmd[REG_EXEC_VERTEX_SHADER_ADDR])
#define REG_UPDATE_RENDER(s)        (s->cmd[REG_UPDATE_RENDER_ADDR])
#define REG_UPDATE_FB(s)            (s->cmd[REG_UPDATE_FB_ADDR])
#define REG_FB_WIDTH(s)             (*(uint32_t*)&s->cmd[REG_FB_WIDTH_ADDR])
#define REG_FB_HEIGHT(s)            (*(uint32_t*)&s->cmd[REG_FB_HEIGHT_ADDR])
#define REG_VERTEX_SIZE(s)          (*(uint32_t*)&s->cmd[REG_VERTEX_SIZE_ADDR])
#define REG_EDGE_SIZE(s)            (*(uint32_t*)&s->cmd[REG_EDGE_SIZE_ADDR])
#define REG_VERTEX_SHADER(s)        (*(uint32_t*)&s->cmd[REG_VERTEX_SHADER_ADDR])
#define REG_FRAGMENT_SHADER(s)      (*(uint32_t*)&s->cmd[REG_FRAGMENT_SHADER_ADDR])
#define REG_EXEC_FRAGMENT_SHADER(s) (s->cmd[REG_EXEC_FRAGMENT_SHADER_ADDR])


#define REG_RB_HEAD(s)       (*(uint32_t*)&s->cmd[REG_RING_BUFFER_HEAD])
#define REG_RB_TAIL(s)       (*(uint32_t*)&s->cmd[REG_RING_BUFFER_TAIL])
#define REG_RB_START(s)      (*(uint32_t*)&s->cmd[REG_RING_BUFFER_START])
#define REG_RB_END(s)        (*(uint32_t*)&s->cmd[REG_RING_BUFFER_END])



typedef struct GpuState {
    
    PCIDevice pdev;
    MemoryRegion mmiomem;         // BAR0: MMIO
    MemoryRegion vrammem;         // BAR1: VRAM
    QemuConsole *con;             
    QEMUTimer *timer;

    uint8_t *vram_ptr;            


    uint32_t gpu_mode;            
    uint32_t ring_buffer_head;
    uint32_t ring_buffer_tail;
    uint32_t ring_buffer_start;
    uint32_t ring_buffer_end;
    uint32_t vs_code_addr;        // 0x10
    uint32_t fs_code_addr;        // 0x14
    uint32_t width;               // 0x18
    uint32_t height;              // 0x1C
    uint32_t framebuffer_vram_offset; // 0x20
    uint32_t gpu_time;            // 0x24 
    uint32_t zbuffer_addr;            // 0x28

    GenericBufferConfig vbo_config;
    GenericBufferConfig edge_config;
    GenericBufferConfig uinform_config;

    Mat4 regs[REG_MAT_SIZE];        
    Preg pRegs[REG_P_GEN_SIZE];
                     
    // fragment pseudo regs
    float px, py;                  
    uint32_t pr, pg, pb;   
    // vector pseudo regs
    Mat4 v_out;
    Mat4 v_pos;
    
    uint8_t cFlag;            
} GpuState;


#ifdef DEBUG_P_REG
#define PRINT_P(OP, A, B, RES, TYPE) \
    do { \
        if ((TYPE) == OP_TYPE_F32 || (TYPE) == OP_TYPE_PF) { \
            printf("[P-DBG] %s: %f , %f -> %f\n", \
                   (OP), (A).f32, (B).f32, (RES).f32); \
        } else { \
            printf("[P-DBG] %s: %u , %u -> %u\n", \
                   (OP), (A).u32, (B).u32, (RES).u32); \
        } \
    } while(0)
#else
#define PRINT_P(OP, A, B, RES, TYPE)  do{}while(0)
#endif

#endif