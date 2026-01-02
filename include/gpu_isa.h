#ifndef ISA_H
#define ISA_H

#include <stdint.h>

typedef struct { float x, y, z; uint32_t rgba; } Vec3;
typedef struct { float x, y, z; } Vec3Raw;
typedef struct { uint32_t a, b; } Edge;
typedef struct { uint32_t a, b, c; } Triangle;
typedef struct { float x, y, z, w;  } Vec4;
typedef struct {uint32_t a_col, b_col, c_col;} Col3;
typedef union {
    float m[4][4]; 
    float elements[16];
    Vec4 rows[4]; 
    Vec3Raw vec3;
    struct {
        Vec4 right;
        Vec4 up;
        Vec4 forward;
        Vec4 position;
    };
} Mat4;

#define PI 3.14159265358979323846

#define REG_MAT_SIZE   8
#define REG_P_SIZE     13
#define REG_P_GEN_SIZE 13


typedef enum {
    C_FLAG_UNUSED,
    C_FLAG_EQ,
    C_FLAG_NEQ,    
    C_FLAG_LT,     
    C_FLAG_GT,     
    C_FLAG_LTE,    
    C_FLAG_GTE,    
    C_FLAGS_NUM,   
    C_FLAG_ENABLE, 
    C_FLAG_DISABLE,
} CFlags;

typedef enum {
    REG_P0,
    REG_P1,
    REG_P2,
    REG_P3,
    REG_P4,
    REG_P5,
    REG_P6,
    REG_P7,
    REG_PX,
    REG_PY,
    REG_PR,
    REG_PG,
    REG_PB
} RegsP;

typedef enum {
    REG_M0, 
    REG_M1, 
    REG_M2, 
    REG_M3, 
    REG_M4, 
    REG_M5, 
    REG_M6, 
    REG_M7, 
    REG_M_IN
} RegsM;


typedef enum {
    INSTR_MOV = 0,  
    INSTR_MUL,  
    INSTR_ROTX, 
    INSTR_ROTY, 
    INSTR_IDENT, 
    INSTR_TRANS, 
    INSTR_MVP,   
    INSTR_EXIT,  
    INSTR_CMP,   
    INSTR_ADD,   
    INSTR_SUB,   
    INSTR_DIV,   
    INSTR_MOD,    
    INSTR_COL,    
    INSTR_FSAN,   
    INSTR_BLEND,  
    INSTR_LERP,   
    INSTR_ABS,   
    INSTR_SQRT,   
    INSTR_SIN,    
    INSTR_COS,    
    INSTR_CAST,  
    INSTR_LDU,
    INSTR_JMP,
    INSTR_AND,
    INSTR_OR,
    INSTR_XOR,
    INSTR_NOT,
    INSTR_PCMP,
    //ADD INSTRUCTIONS
    INSTR_NORM, 
    INSTR_MIN,
    INSTR_MAX,
    INSTR_CLAMP,   // min(max(x,a),b)
    INSTR_NEG,     // -x
    INSTR_RECIP,   // 1/x (szybsze niż DIV)
    INSTR_RSQRT,   // 1/sqrt(x)
    INSTR_DOT,     // dot(v1, v2)
    INSTR_CROSS,   // cross(v1, v2)
    INSTR_LEN,     // length(v)
    INSTR_FMA,     // fused multiply-add (a*b + c)
    INSTR_MAD,     // multiply-add
    INSTR_SAT,     // saturate [0,1]
    INSTR_SIGN,    // sign(x)
    INSTR_VEC3,
    INSTR_TAN,
    INSTR_ATAN,
    INSTR_EXP
} InstrOpcode;


typedef enum {
    ARG_TYPE_IMM,
    ARG_TYPE_REG,   
    ARG_TYPE_DATA  
} ArgType;

typedef enum {
    OP_TYPE_U32,
    OP_TYPE_F32,
    OP_TYPE_MATRIX,
    OP_TYPE_VEC4,
    OP_TYPE_VEC3,
} OpType;

typedef union {
    uint32_t u32;
    float    f32;
} FI32;

typedef FI32 Preg;
typedef FI32 InstrArg;

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

#endif