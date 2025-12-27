#ifndef COMPILER
#define COMPILER
#include "isa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"

#define TOKEN_MOVM "movm"
#define TOKEN_MOV  "mov"
#define TOKEN_ROTX "rotx"
#define TOKEN_ROTY "roty"
#define TOKEN_IDENT "ident"
#define TOKEN_TRANS "trans"
#define TOKEN_MVP "mvp"
#define TOKEN_EXIT "exit"
#define TOKEN_CMPI "cmpi"
#define TOKEN_CMPF "cmpf"
#define TOKEN_MULM "mulm"
#define TOKEN_MULV "mulv"
#define TOKEN_MULI "muli"
#define TOKEN_MULF "mulf"
#define TOKEN_MULV3 "mulv3"
#define TOKEN_ADDI "addi"
#define TOKEN_ADDF "addf"
#define TOKEN_ADDV "addv"
#define TOKEN_ADDV3 "addv3"
#define TOKEN_SUBI "subi"
#define TOKEN_SUBF "subf"
#define TOKEN_SUBV "subv"
#define TOKEN_SUBV3 "subv3"
#define TOKEN_DIVI "divi"
#define TOKEN_DIVF "divf"
#define TOKEN_MOD "mod"
#define TOKEN_COL "col"
#define TOKEN_FSAN "fsan"
#define TOKEN_BLENDI "blendi"
#define TOKEN_BLENDF "blendf"
#define TOKEN_LERPI "lerpi"
#define TOKEN_LERPF "lerpf"
#define TOKEN_ABSI "absi"
#define TOKEN_ABSF "absf"
#define TOKEN_SQRT "sqrt"
#define TOKEN_SIN "sin"
#define TOKEN_COS "cos"
#define TOKEN_CASTI "casti"
#define TOKEN_CASTF "castf"
#define TOKEN_LDUM "ldum"
#define TOKEN_LDUV "lduv"
#define TOKEN_LDUF "lduf"
#define TOKEN_LDUI "ldui"
#define TOKEN_PRINT "print"
#define TOKEN_JMP   "jmp"
#define TOKEN_AND   "and"
#define TOKEN_OR   "or"
#define TOKEN_NOT   "not"
#define TOKEN_XOR   "xor"
#define TOKEN_PCMPI   "pcmpi"
#define TOKEN_PCMPF   "pcmpf"


#define TOKEN_REG_M 'm'
#define TOKEN_REG_M_IN "mv"
#define TOKEN_REG_P 'p'

#define TOKEN_REG_PX "px"
#define TOKEN_REG_PY "py"
#define TOKEN_REG_PR "pr"
#define TOKEN_REG_PG "pg"
#define TOKEN_REG_PB "pb"

#define TOKEN_C_FLAG_ENABLE '!'
#define TOKEN_C_FLAG_EQ     "eq"
#define TOKEN_C_FLAG_NEQ    "neq"
#define TOKEN_C_FLAG_LT     "lt"
#define TOKEN_C_FLAG_GT     "gt"
#define TOKEN_C_FLAG_LTE    "lte"
#define TOKEN_C_FLAG_GTE    "gte"

typedef enum {
    FORMAT_NONE,    // Instruction with no args (exit)
    FORMAT_D,       // Destination only (ident, fsan, mvp)
    FORMAT_A,       // Argument only (print)
    FORMAT_D_A,     // Dest + 1 Arg (mov, rotx, abs, sqrt, cast, ldu)
    FORMAT_D_A_B,   // Dest + 2 Args (mul, add, sub, div, mod)
    FORMAT_D_A_B_C, // Dest + 3 Args (trans, col, blend, lerp)
    FORMAT_CMP,     // Special: Opcode + Cflag + 2 Args (cmp)
    FORMAT_PCMP,     // Special: Opcode + Cflag + Dest + 2 Args (cmp)
    FORMAT_JMP      // Opcode + 1 Arg (Address/Label)
} InstFormat;

typedef struct {
    const char* token;
    uint8_t opcode;
    uint8_t opType;
    uint8_t argc;
} TokenDef;

TokenDef token_defs[] = {
    { TOKEN_MOVM,  INSTR_MOV,   OP_TYPE_MATRIX, 1 },
    { TOKEN_MOV,   INSTR_MOV,   OP_TYPE_U32,    1 },
    { TOKEN_ROTX,  INSTR_ROTX,  OP_TYPE_MATRIX, 1 },
    { TOKEN_ROTY,  INSTR_ROTY,  OP_TYPE_MATRIX, 1 },
    { TOKEN_IDENT, INSTR_IDENT, OP_TYPE_MATRIX, 0 },
    { TOKEN_TRANS, INSTR_TRANS, OP_TYPE_MATRIX, 3 },
    { TOKEN_MVP,   INSTR_MVP,   OP_TYPE_MATRIX, 0 },
    { TOKEN_EXIT,  INSTR_EXIT,  OP_TYPE_U32,    0 },
    { TOKEN_CMPI,  INSTR_CMP,   OP_TYPE_U32,    2 },
    { TOKEN_CMPF,  INSTR_CMP,   OP_TYPE_F32,    2 },
    { TOKEN_MULM,  INSTR_MUL,   OP_TYPE_MATRIX, 2 },
    { TOKEN_MULV,  INSTR_MUL,   OP_TYPE_VEC4,   2 },
    { TOKEN_MULI,  INSTR_MUL,   OP_TYPE_U32,    2 },
    { TOKEN_MULF,  INSTR_MUL,   OP_TYPE_F32,    2 },
    { TOKEN_ADDI,  INSTR_ADD,   OP_TYPE_U32,    2 },
    { TOKEN_ADDF,  INSTR_ADD,   OP_TYPE_F32,    2 },
    { TOKEN_SUBI,  INSTR_SUB,   OP_TYPE_U32,    2 },
    { TOKEN_SUBF,  INSTR_SUB,   OP_TYPE_F32,    2 },
    { TOKEN_DIVI,  INSTR_DIV,   OP_TYPE_U32,    2 },
    { TOKEN_DIVF,  INSTR_DIV,   OP_TYPE_F32,    2 },
    { TOKEN_MOD,   INSTR_MOD,   OP_TYPE_U32,    2 },
    { TOKEN_COL,   INSTR_COL,   OP_TYPE_U32,    3 },
    { TOKEN_FSAN,  INSTR_FSAN,  OP_TYPE_F32,    0 },
    { TOKEN_BLENDI,INSTR_BLEND, OP_TYPE_U32,    3 },
    { TOKEN_BLENDF,INSTR_BLEND, OP_TYPE_F32,    3 },
    { TOKEN_LERPI, INSTR_LERP,  OP_TYPE_U32,    3 },
    { TOKEN_LERPF, INSTR_LERP,  OP_TYPE_F32,    3 },
    { TOKEN_ABSI,  INSTR_ABS,   OP_TYPE_U32,    1 },
    { TOKEN_ABSF,  INSTR_ABS,   OP_TYPE_F32,    1 },
    { TOKEN_SQRT,  INSTR_SQRT,  OP_TYPE_F32,    1 },
    { TOKEN_SIN,   INSTR_SIN,   OP_TYPE_F32,    1 },
    { TOKEN_COS,   INSTR_COS,   OP_TYPE_F32,    1 },
    { TOKEN_CASTI, INSTR_CAST,  OP_TYPE_U32,    1 },
    { TOKEN_CASTF, INSTR_CAST,  OP_TYPE_F32,    1 },
    { TOKEN_LDUM,  INSTR_LDU,   OP_TYPE_MATRIX, 1 },
    { TOKEN_LDUV,  INSTR_LDU,   OP_TYPE_VEC4,   1 },
    { TOKEN_LDUI,  INSTR_LDU,   OP_TYPE_U32,    1 },
    { TOKEN_LDUF,  INSTR_LDU,   OP_TYPE_F32,    1 }
};

int num_tokens = sizeof(token_defs)/sizeof(token_defs[0]);
typedef struct  {
    char* token;
    uint8_t code;
} C_FLAG_MAP;
C_FLAG_MAP cflag_defs[] = {
    { TOKEN_C_FLAG_EQ, C_FLAG_EQ   },     
    { TOKEN_C_FLAG_NEQ, C_FLAG_NEQ },    
    { TOKEN_C_FLAG_LT, C_FLAG_LT   },     
    { TOKEN_C_FLAG_GT, C_FLAG_GT   },     
    { TOKEN_C_FLAG_LTE, C_FLAG_LTE },    
    { TOKEN_C_FLAG_GTE, C_FLAG_GTE }
};


typedef struct  {
    uint8_t  opcode;
    uint8_t  opType;
    uint8_t  isCflag;
    uint8_t  argc;
} op_def;

typedef struct
{
    uint8_t  type;
    FI32 val;
} arg_data;

typedef struct {
    char* name;
    uint32_t address;
} Label;

typedef struct {
    Vector *tokens;
    size_t cursor;
    FILE *output;
    int has_error;
    int verbose;

    Label* labels;
    size_t label_count;
    size_t label_capacity;
    uint32_t current_pc;


} Assembler;

int tokenize_file(const char *filename, Vector *v);
int process_tokens(Vector *tokens);
#endif