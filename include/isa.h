#ifndef ISA_H
#define ISA_H

#include <stdint.h>

typedef struct { float x, y, z; uint32_t rgba; } Vec3;
typedef struct { uint32_t a, b; } Edge;
typedef struct { uint32_t a, b, c; } Triangle;
typedef struct { float x, y, z, w;  } Vec4;
typedef struct {uint32_t a_col, b_col, c_col;} Col3;
typedef union {
    float m[4][4]; 
    float elements[16];
    Vec4 rows[4]; 
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
    INSTR_LDU
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




#define MAX_UNIFORMS_PER_SHADER  16
#define MAX_ATTRIBUTES_PER_SHADER 8

typedef enum {
    D_TYPE_FLOAT = 1,
    D_TYPE_VEC2,
    D_TYPE_VEC3,
    D_TYPE_VEC4,
    D_TYPE_MAT4,
    D_TYPE_UINT32
} DataType;

typedef struct {
    uint32_t vbo_addr;          
    uint32_t size;             
    DataType element_type;     
    
} GenericBufferConfig;


typedef struct {
    DataType data_type;        
    uint32_t offset; 
} AttributeMap;

typedef struct __attribute__((packed)) {
    DataType data_type;
    uint32_t offset_in_buffer; 
} ShaderResourceMap;



typedef struct __attribute__((packed)) {
    uint32_t shader_type;       
    uint32_t num_instructions;
    
    uint32_t num_uniforms;
    ShaderResourceMap uniform_map[MAX_UNIFORMS_PER_SHADER]; 
    
    uint32_t num_attributes;
    AttributeMap attribute_map[MAX_ATTRIBUTES_PER_SHADER]; 

} Shader_Header;

typedef struct
{
    Shader_Header header_section;
    uint32_t code_section;
} Shader;



typedef enum {
    CMD_NOOP               = 0x00, 
    CMD_DRAW_PRIMITIVE     = 0x01, 
    CMD_SET_STATE          = 0x02,
    CMD_CLEAR_FRAMEBUFFER  = 0x03,
} CommandOpcode;


typedef enum {
    STATE_ID_VBO_CONFIG = 1,
    STATE_ID_EDGE_CONFIG,   
    STATE_ID_UNIFORM_CONFIG,
    STATE_ID_SHADER_PTRS,    
} StateID;


typedef enum {
    PRIMITIVE_TYPE_POINTS  = 0x01, //TO-DO IN FUTURE
    PRIMITIVE_TYPE_LINES   = 0x02,
    PRIMITIVE_TYPE_TRIANGLES = 0x03, // TO-DO IN FUTURE
} PrimitiveType;


typedef struct __attribute__((packed)) {
    PrimitiveType type;         
} DrawPrimitivePayload;

typedef struct __attribute__((packed)) {
    StateID state_id; 
    
    union {
        GenericBufferConfig buffer_config;
        struct __attribute__((packed)) {
            uint32_t vs_addr;             
            uint32_t fs_addr;
        } shader_ptrs;
        
    } value;
} SetStatePayload;

typedef struct __attribute__((packed)) {
    uint8_t options; // TO-DO in future
} ClearFramebufferPayload;

typedef struct __attribute__((packed)) {
    CommandOpcode opcode;

    union {
        DrawPrimitivePayload draw;
        SetStatePayload state;
        ClearFramebufferPayload clear;
        uint32_t raw_data[8]; 
    } payload;
} Command;


#endif