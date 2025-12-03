#ifndef ISA_H
#define ISA_H

#include <stdint.h>

#define TYPE_PCI_GPU_DEVICE "AREK"
#define GPU_DEVICE_ID       0x2137
#define PCI_VENDOR_ID_CUSTOM 0x6969

#define GPU_MMIO_BAR    0
#define GPU_VRAM_BAR    1

#define GPU_FB_WIDTH  640
#define GPU_FB_HEIGHT 480

#define GPU_VRAM_SIZE (1 << 25)   /* 32 MB */
#define GPU_CMD_SIZE  0x1000      /* BAR0 command size */

#define GPU_VRAM_FB_SEGMENT_ADDR      0x0000000
#define GPU_VRAM_VERTEX_SEGMENT_ADDR  0x0800000
#define GPU_VRAM_EDGES_SEGMENT_ADDR   0x1000000
#define GPU_VRAM_SHADER_SEGMENT_ADDR  0x1800000

#define REG_GPU_MODE_ADDR              0
#define REG_EXEC_VERTEX_SHADER_ADDR    1
#define REG_UPDATE_RENDER_ADDR         2
#define REG_UPDATE_FB_ADDR             3
#define REG_FB_WIDTH_ADDR              4
#define REG_FB_HEIGHT_ADDR             8
#define REG_VERTEX_SIZE_ADDR           12
#define REG_EDGE_SIZE_ADDR             16
#define REG_VERTEX_SHADER_ADDR         20
#define REG_FRAGMENT_SHADER_ADDR       24

#define GPU_MODE_GOP 0
#define GPU_MODE_3D  1

typedef struct { double x, y, z; uint32_t rgba; } Vec3;
typedef struct { uint32_t a, b; } Edge;
typedef struct { double m[4][4]; } Mat4;
typedef struct { double x, y, z, w; } Vec4;

#define PI 3.14159265358979323846

#define REG_MAT_SIZE   8
#define REG_P_SIZE     13
#define REG_P_GEN_SIZE 8
#define REG_MAT_NUM_OK(n) ((n) < REG_MAT_SIZE)
#define REG_P_NUM_OK(n)   ((n) < REG_P_SIZE)

#define REG_NUM(r) ((r) & 0x7FFFFFFF)

#define C_FLAG_UNUSED 0
#define C_FLAG_EQ     1
#define C_FLAG_NEQ    2
#define C_FLAG_LT     3
#define C_FLAG_GT     4
#define C_FLAG_LTE    5
#define C_FLAG_GTE    6
#define C_FLAGS_NUM   7

#define C_FLAG_ENABLE  8
#define C_FLAG_DISABLE 9

#define REG_P0 0
#define REG_P1 1
#define REG_P2 2
#define REG_P3 3
#define REG_P4 4
#define REG_P5 5
#define REG_P6 6
#define REG_P7 7

#define REG_PX 8
#define REG_PY 9
#define REG_PR 10
#define REG_PG 11
#define REG_PB 12

#define REG_MN(n) (n)
#define REG_M0 REG_MN(0)
#define REG_M1 REG_MN(1)
#define REG_M2 REG_MN(2)
#define REG_M3 REG_MN(3)
#define REG_M4 REG_MN(4)
#define REG_M5 REG_MN(5)
#define REG_M6 REG_MN(6)
#define REG_M7 REG_MN(7)

#define ARG_IS_MEM_ADDR(arg) (((arg) >> 31) == 0)

typedef union {
    uint32_t u32;
    float    f32;
} FI32;

typedef FI32 Preg;

typedef FI32 InstrArg;

#define INSTR_MOV  0
#define INSTR_MUL  1
#define INSTR_ROTX 2
#define INSTR_ROTY 3
#define INSTR_IDENT 4
#define INSTR_TRANS 5
#define INSTR_MVP  6
#define INSTR_EXIT 7
#define INSTR_CMP  8
#define INSTR_ADD  9
#define INSTR_SUB  10
#define INSTR_DIV  11
#define INSTR_MOD  12 // I op
#define INSTR_COL  13 // REG op
#define INSTR_FSAN 14 // Reg F op
#define INSTR_BLEND 15 // !
#define INSTR_LERP  16 // !
#define INSTR_ABS   17 // Reg I F op
#define INSTR_SQRT  18 // F op
#define INSTR_SIN   19 // F op
#define INSTR_COS   20 // F op
#define INSTR_CAST  21 // REG I F op

typedef struct  Instr {
    uint8_t  opcode;
    uint8_t  cFlag;
    uint8_t  dest;
    uint8_t  arg0Type:2;
    uint8_t  arg1Type:2;
    uint8_t  arg2Type:2;
    uint8_t  opType:2;
    InstrArg arg0;
    InstrArg arg1;
    InstrArg arg2;
} Instr;

#define ARG_TYPE_IMM   0
#define ARG_TYPE_REG   1
#define ARG_TYPE_DATA  2

#define OP_TYPE_U32      0
#define OP_TYPE_F32      1
#define OP_TYPE_MATRIX   2

#endif