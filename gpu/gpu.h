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

#include "isa.h"
#ifndef GPU_H
#define GPU_H

#include "hw/pci/pci.h"
#include "isa.h"


#define GPU_VRAM_FB_SEGMENT(s)      (&((s)->vram_ptr[GPU_VRAM_FB_SEGMENT_ADDR]))
#define GPU_VRAM_VERTEX_SEGMENT(s)  (&((s)->vram_ptr[GPU_VRAM_VERTEX_SEGMENT_ADDR]))
#define GPU_VRAM_EDGES_SEGMENT(s)   (&((s)->vram_ptr[GPU_VRAM_EDGES_SEGMENT_ADDR]))
#define GPU_VRAM_SHADER_SEGMENT(s)  (&((s)->vram_ptr[GPU_VRAM_SHADER_SEGMENT_ADDR]))

#define FB(s)            ((uint32_t*) GPU_VRAM_FB_SEGMENT(s))
#define VERTEX_TABLE(s)  ((Vec3*)     GPU_VRAM_VERTEX_SEGMENT(s))
#define EDGES_TABLE(s)   ((Edge*)     GPU_VRAM_EDGES_SEGMENT(s))
#define SHADER_PROGRAM(s)((void*)     GPU_VRAM_SHADER_SEGMENT(s))


#define REG_GPU_MODE(s)           (s->cmd[REG_GPU_MODE_ADDR])
#define REG_EXEC_VERTEX_SHADER(s) (s->cmd[REG_EXEC_VERTEX_SHADER_ADDR])
#define REG_UPDATE_RENDER(s)      (s->cmd[REG_UPDATE_RENDER_ADDR])
#define REG_UPDATE_FB(s)          (s->cmd[REG_UPDATE_FB_ADDR])
#define REG_FB_WIDTH(s)           (*(uint32_t*)&s->cmd[REG_FB_WIDTH_ADDR])
#define REG_FB_HEIGHT(s)          (*(uint32_t*)&s->cmd[REG_FB_HEIGHT_ADDR])
#define REG_VERTEX_SIZE(s)        (*(uint32_t*)&s->cmd[REG_VERTEX_SIZE_ADDR])
#define REG_EDGE_SIZE(s)          (*(uint32_t*)&s->cmd[REG_EDGE_SIZE_ADDR])
#define REG_VERTEX_SHADER(s)      (*(uint32_t*)&s->cmd[REG_VERTEX_SHADER_ADDR])

typedef struct GpuState {
    PCIDevice pdev;
    MemoryRegion cmdmem;   /* BAR0 */
    MemoryRegion vrammem;  /* BAR1 */
    QemuConsole *con;
    QEMUTimer *timer;

    uint8_t cmd[GPU_CMD_SIZE];
    uint8_t *vram_ptr;

    Mat4 regs[REG_MAT_SIZE];
    Preg pRegs[REG_P_SIZE];
    Mat4 mvp;
    uint32_t px,py,pr,pg,pb;
    uint8_t cFlag;
} GpuState;



//EXEC SHADER HELPERS

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

#define CHECK_PREG(n)  if(!REG_P_NUM_OK(n)){ printf("BAD pReg %u\n", n); end = 0; break; }

#define PREG_A   gpu->pRegs[instr.arg0.u32]
#define PREG_B   gpu->pRegs[instr.arg1.u32]
#define PREG_DST gpu->pRegs[instr.dst]

#define DO_PF(op)  (PREG_DST.f32 = (PREG_A.f32 op PREG_B.f32))
#define DO_F32(op) (PREG_DST.f32 = (PREG_A.f32 op instr.arg1.f32))
#define DO_PI(op)  (PREG_DST.u32 = (PREG_A.u32 op PREG_B.u32))
#define DO_I32(op) (PREG_DST.u32 = (PREG_A.u32 op instr.arg1.u32))


#define EXEC_P_OP(OPNAME, OPERATOR)                                \
    CHECK_PREG(instr.arg0.u32);                                    \
    CHECK_PREG(instr.dst);                                         \
    switch(instr.opType)                                           \
    {                                                              \
        case OP_TYPE_PF:                                           \
            CHECK_PREG(instr.arg1.u32);                            \
            DO_PF(OPERATOR);                                       \
            PRINT_P(OPNAME, PREG_A, PREG_B, PREG_DST, OP_TYPE_PF); \
            break;                                                 \
                                                                   \
        case OP_TYPE_PI:                                           \
            CHECK_PREG(instr.arg1.u32);                            \
            DO_PI(OPERATOR);                                       \
            PRINT_P(OPNAME, PREG_A, PREG_B, PREG_DST, OP_TYPE_PI); \
            break;                                                 \
                                                                   \
        case OP_TYPE_F32:                                          \
            DO_F32(OPERATOR);                                      \
            PRINT_P(OPNAME, PREG_A, PREG_B, PREG_DST, OP_TYPE_F32);\
            break;                                                 \
                                                                   \
        case OP_TYPE_I32:                                          \
            DO_I32(OPERATOR);                                      \
            PRINT_P(OPNAME, PREG_A, PREG_B, PREG_DST, OP_TYPE_I32);\
            break;                                                 \
                                                                   \
        default:                                                   \
            printf("BAD OP_TYPE in %s\n", OPNAME);                 \
            break;                                                 \
    }

#endif
