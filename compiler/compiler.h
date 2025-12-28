#ifndef COMPILER
#define COMPILER

#include "isa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"

// Token Definitions
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
#define TOKEN_NORMV3   "normv3"

#define TOKEN_MINI   "mini"
#define TOKEN_MINF   "minf"
#define TOKEN_MAXI   "maxi"
#define TOKEN_MAXF   "maxf"
#define TOKEN_CLAMPI   "clampi"
#define TOKEN_CLAMPF   "clampf"
#define TOKEN_NEGI   "negi"
#define TOKEN_NEGF   "negf"
#define TOKEN_RECIPI  "recipi"
#define TOKEN_RECIPF "recipf"
#define TOKEN_RSQRTF "rsqrtf"
#define TOKEN_DOTV3  "dotv3"
#define TOKEN_CROSSV3 "crossv3"
#define TOKEN_LENV3  "lenv3"
#define TOKEN_FMAF   "fmaf"
#define TOKEN_FMAI   "fmai"
#define TOKEN_MADI   "madi"
#define TOKEN_MADF   "madf"
#define TOKEN_SATF   "satf"
#define TOKEN_SIGNI  "signi"
#define TOKEN_SIGNF  "signf"
#define TOKEN_SIGNV3  "signv3"
#define TOKEN_VEC3 "vec3"
#define TOKEN_TAN "tan"
#define TOKEN_ATAN "atan"


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
InstFormat format;
} TokenDef;

typedef struct {
uint8_t type;
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
void assemble(Assembler *as);

#endif