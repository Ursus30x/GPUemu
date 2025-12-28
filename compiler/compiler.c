#define VECTOR_IMPLEMENTATION
#include "compiler.h"
#include <ctype.h>

static const TokenDef token_defs[] = {
    { TOKEN_MOVM,   INSTR_MOV,   OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_MOV,    INSTR_MOV,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_ROTX,   INSTR_ROTX,  OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_ROTY,   INSTR_ROTY,  OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_IDENT,  INSTR_IDENT, OP_TYPE_MATRIX, 0, FORMAT_D },
    { TOKEN_TRANS,  INSTR_TRANS, OP_TYPE_MATRIX, 3, FORMAT_D_A_B_C },
    { TOKEN_MVP,    INSTR_MVP,   OP_TYPE_MATRIX, 0, FORMAT_D },
    { TOKEN_EXIT,   INSTR_EXIT,  OP_TYPE_U32,    0, FORMAT_NONE },
    { TOKEN_CMPI,   INSTR_CMP,   OP_TYPE_U32,    2, FORMAT_CMP },
    { TOKEN_CMPF,   INSTR_CMP,   OP_TYPE_F32,    2, FORMAT_CMP },
    { TOKEN_MULM,   INSTR_MUL,   OP_TYPE_MATRIX, 2, FORMAT_D_A_B },
    { TOKEN_MULV,   INSTR_MUL,   OP_TYPE_VEC4,   2, FORMAT_D_A_B },
    { TOKEN_MULV3,  INSTR_MUL,   OP_TYPE_VEC3,   2, FORMAT_D_A_B },
    { TOKEN_MULI,   INSTR_MUL,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_MULF,   INSTR_MUL,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_ADDI,   INSTR_ADD,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_ADDF,   INSTR_ADD,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_ADDV,   INSTR_ADD,   OP_TYPE_VEC4,   2, FORMAT_D_A_B },
    { TOKEN_ADDV3,  INSTR_ADD,   OP_TYPE_VEC3,   2, FORMAT_D_A_B },
    { TOKEN_SUBI,   INSTR_SUB,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_SUBF,   INSTR_SUB,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_SUBV,   INSTR_SUB,   OP_TYPE_VEC4,   2, FORMAT_D_A_B },
    { TOKEN_SUBV3,  INSTR_SUB,   OP_TYPE_VEC3,   2, FORMAT_D_A_B },
    { TOKEN_DIVI,   INSTR_DIV,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_DIVF,   INSTR_DIV,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_MULV,   INSTR_MUL,   OP_TYPE_VEC4,   2, FORMAT_D_A_B },
    { TOKEN_MULV3,  INSTR_MUL,   OP_TYPE_VEC3,   2, FORMAT_D_A_B },
    { TOKEN_MOD,    INSTR_MOD,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_COL,    INSTR_COL,   OP_TYPE_U32,    3, FORMAT_D_A_B_C },
    { TOKEN_FSAN,   INSTR_FSAN,  OP_TYPE_F32,    0, FORMAT_D },
    { TOKEN_BLENDI, INSTR_BLEND, OP_TYPE_U32,    3, FORMAT_D_A_B_C },
    { TOKEN_BLENDF, INSTR_BLEND, OP_TYPE_F32,    3, FORMAT_D_A_B_C },
    { TOKEN_LERPI,  INSTR_LERP,  OP_TYPE_U32,    3, FORMAT_D_A_B_C },
    { TOKEN_LERPF,  INSTR_LERP,  OP_TYPE_F32,    3, FORMAT_D_A_B_C },
    { TOKEN_ABSI,   INSTR_ABS,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_ABSF,   INSTR_ABS,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_SQRT,   INSTR_SQRT,  OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_SIN,    INSTR_SIN,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_COS,    INSTR_COS,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_CASTI,  INSTR_CAST,  OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_CASTF,  INSTR_CAST,  OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_LDUM,   INSTR_LDU,   OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_LDUV,   INSTR_LDU,   OP_TYPE_VEC4,   1, FORMAT_D_A },
    { TOKEN_LDUI,   INSTR_LDU,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_LDUF,   INSTR_LDU,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_JMP,    INSTR_JMP,   OP_TYPE_U32,    1, FORMAT_JMP },
    { TOKEN_AND,    INSTR_AND,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_OR,     INSTR_OR,    OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_NOT,    INSTR_NOT,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_XOR,    INSTR_XOR,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_PCMPI,  INSTR_PCMP,  OP_TYPE_U32,    2, FORMAT_PCMP },
    { TOKEN_PCMPF,  INSTR_PCMP,  OP_TYPE_F32,    2, FORMAT_PCMP },
    { TOKEN_NORMV3, INSTR_NORM,  OP_TYPE_VEC4,   1, FORMAT_D_A },
    { TOKEN_MINI,   INSTR_MIN,   OP_TYPE_U32,    2, FORMAT_D_A_B  },
    { TOKEN_MAXI,   INSTR_MAX,   OP_TYPE_U32,    2, FORMAT_D_A_B  },
    { TOKEN_MINF,   INSTR_MIN,   OP_TYPE_F32,    2, FORMAT_D_A_B  },
    { TOKEN_MAXF,   INSTR_MAX,   OP_TYPE_F32,    2, FORMAT_D_A_B  },
    { TOKEN_CLAMPI, INSTR_CLAMP, OP_TYPE_U32,    3, FORMAT_D_A_B_C},
    { TOKEN_CLAMPF, INSTR_CLAMP, OP_TYPE_F32,    3, FORMAT_D_A_B_C},
    { TOKEN_NEGI,   INSTR_NEG,   OP_TYPE_U32,    1, FORMAT_D_A    },
    { TOKEN_NEGF,   INSTR_NEG,   OP_TYPE_F32,    1, FORMAT_D_A    },
    { TOKEN_RECIPF, INSTR_RECIP, OP_TYPE_F32,    1, FORMAT_D_A    },
    { TOKEN_RECIPI, INSTR_RECIP, OP_TYPE_U32,    1, FORMAT_D_A    },
    { TOKEN_RSQRTF, INSTR_RSQRT, OP_TYPE_F32,    1, FORMAT_D_A    },
    { TOKEN_DOTV3,  INSTR_DOT,   OP_TYPE_VEC3,   2, FORMAT_D_A_B  },
    { TOKEN_CROSSV3,INSTR_CROSS, OP_TYPE_VEC3,   2, FORMAT_D_A_B  },
    { TOKEN_LENV3,  INSTR_LEN,   OP_TYPE_VEC3,   1, FORMAT_D_A    },
    { TOKEN_FMAF,   INSTR_FMA,   OP_TYPE_F32,    3, FORMAT_D_A_B_C},
    { TOKEN_FMAI,   INSTR_FMA,   OP_TYPE_U32,    3, FORMAT_D_A_B_C},
    { TOKEN_MADI,   INSTR_MAD,   OP_TYPE_U32,    3, FORMAT_D_A_B_C},
    { TOKEN_MADF,   INSTR_MAD,   OP_TYPE_F32,    3, FORMAT_D_A_B_C},
    { TOKEN_SATF,   INSTR_SAT,   OP_TYPE_F32,    1, FORMAT_D_A    },
    { TOKEN_SIGNF,  INSTR_SIGN,  OP_TYPE_F32,    1, FORMAT_D_A    },
    { TOKEN_SIGNI,  INSTR_SIGN,  OP_TYPE_U32,    1, FORMAT_D_A    },
    { TOKEN_SIGNV3, INSTR_SIGN,  OP_TYPE_VEC3,   1, FORMAT_D_A    },
    { TOKEN_VEC3,   INSTR_VEC3,  OP_TYPE_U32,    1, FORMAT_D_A_B_C}, // to do in pregs out m reg
    { TOKEN_TAN,    INSTR_TAN,   OP_TYPE_F32,    1, FORMAT_D_A    },
    { TOKEN_ATAN,   INSTR_ATAN,  OP_TYPE_F32,    2, FORMAT_D_A_B  }
};

static const size_t num_token_defs = sizeof(token_defs) / sizeof(token_defs[0]);

char* my_strdup(const char* s) 
{
    size_t len = strlen(s) + 1;
    char* d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

int tokenize_file(const char *filename, Vector *v) 
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        printf("File \"%s\" doesn't exist!\n", filename);
        exit(EXIT_FAILURE);
    }

    char buffer[256];

    while (fgets(buffer, sizeof(buffer), fp))
    {
        char *token = strtok(buffer, " \n");
        while (token) 
        {

            if (!vector_push(v, token)) 
            {
                fclose(fp);
                return 0;
            }

            token = strtok(NULL, " \n");
        }
    }

    fclose(fp);
    return 1;
}


op_def new_instr(Str10 tok)
{
    op_def def;
    if(tok[0] == TOKEN_C_FLAG_ENABLE)
    {
        def.isCflag = 1;
        memmove(tok, tok + 1, strlen(tok)); 
    }
<<<<<<< HEAD
   
    int found = 0;
    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tok, token_defs[i].token) == 0) {
            def.opcode = token_defs[i].opcode;
            def.opType = token_defs[i].opType;
            def.argc = token_defs[i].argc;
            found = 1;
=======
    as->labels[as->label_count].name = my_strdup(name);
    as->labels[as->label_count].address = address;
    as->label_count++;
}

uint32_t get_label_address(Assembler *as, const char* name) 
{
    for (size_t i = 0; i < as->label_count; i++) 
    {
        if (strcmp(as->labels[i].name, name) == 0) return as->labels[i].address;
    }
    report_error(as, "Undefined label reference");
    return 0;
}

uint8_t parse_reg(Assembler *as, const char* tok, uint8_t opType, int force_scalar) 
{
    if (!tok) report_error(as, "Expected register");
    if ((opType == OP_TYPE_MATRIX || opType == OP_TYPE_VEC4|| opType == OP_TYPE_VEC3) && !force_scalar) 
    {
        if (tok[0] == TOKEN_REG_M) 
        {
            if (strcmp(tok, TOKEN_REG_M_IN) == 0) return REG_M_IN;
            return (uint8_t)atoi(tok + 1);
        }
    } else {
        if (strcmp(tok, TOKEN_REG_PX) == 0) return REG_PX;
        if (strcmp(tok, TOKEN_REG_PY) == 0) return REG_PY;
        if (strcmp(tok, TOKEN_REG_PR) == 0) return REG_PR;
        if (strcmp(tok, TOKEN_REG_PG) == 0) return REG_PG;
        if (strcmp(tok, TOKEN_REG_PB) == 0) return REG_PB;
        if (tok[0] == TOKEN_REG_P) return (uint8_t)atoi(tok + 1);
    }
    report_error(as, "Invalid register");
    return 0;
}

arg_data parse_arg(Assembler *as, const char* tok, const TokenDef *def, int pass) 
{
    if (!tok) report_error(as, "Expected argument");
    arg_data arg;
    char* endptr;

    if (isdigit(tok[0]) || (tok[0] == '-' && isdigit(tok[1]))) 
    {
        arg.type = ARG_TYPE_IMM;
        if (def->opType == OP_TYPE_F32) arg.val.f32 = strtof(tok, &endptr);
        else arg.val.u32 = (uint32_t)strtol(tok, &endptr, 0);
        return arg;
    }
    
    if (def->format == FORMAT_JMP) 
    {
        arg.type = ARG_TYPE_IMM;
        if (pass == 2) arg.val.u32 = get_label_address(as, tok);
        else arg.val.u32 = 0;
        return arg;
    }

    arg.type = ARG_TYPE_REG;
    arg.val.u32 = parse_reg(as, tok, def->opType, 1);
    return arg;
}

void print_instr_debug(const char *tok, const Instr* instr) 
{
    printf("[DEBUG] %-10s | Opcode: %u, CFlag: %u, Dest: %u, Arg0: 0x%08x, Arg1: 0x%08x\n", 
           tok, instr->opcode, instr->cFlag, instr->dest, instr->arg0.u32, instr->arg1.u32);
}

void process_instruction(Assembler *as, int pass) 
{
    const char* head = consume(as);
    if (!head) return;

    size_t len = strlen(head);
    if (head[len - 1] == ':') 
    {
        if (pass == 1) 
        {
            char* name = my_strdup(head);
            name[len - 1] = '\0';
            add_label(as, name, as->current_pc);
            free(name);
        }
        return;
    }

    uint8_t is_cond = (head[0] == TOKEN_C_FLAG_ENABLE);
    const char* op_str = is_cond ? head + 1 : head;

    const TokenDef *def = NULL;
    for (size_t i = 0; i < num_token_defs; i++) 
    {
        if (strcmp(op_str, token_defs[i].token) == 0) 
        {
            def = &token_defs[i];
>>>>>>> 9b768a5 ([Compiler] New ISA instr)
            break;
        }
    }

    if (!found) {
        printf("Unknown token: %s\n", tok);
        exit(EXIT_FAILURE);
    }
    return def;
}
void printInstr(const Instr* instr) {
    printf("Instr {\n");
    printf("  opcode: %u\n", instr->opcode);
    printf("  cFlag: %u\n", instr->cFlag);
    printf("  dest: %u\n", instr->dest);
    printf("  arg0Type: %u\n", instr->arg0Type);
    printf("  arg1Type: %u\n", instr->arg1Type);
    printf("  arg2Type: %u\n", instr->arg2Type);
    printf("  opType: %u\n", instr->opType);
    printf("  arg0: %x\n", instr->arg0.u32);
    printf("  arg1: %x\n", instr->arg1.u32);
    printf("  arg2: %x\n", instr->arg2.u32); 
    printf("}\n");
}
void parseInstr(const Instr* instr)
{
    printInstr(instr);
 
    uint64_t *instrBin = (uint64_t*)instr;
    for(size_t i = 0; i < sizeof(Instr)/sizeof(uint64_t); i++)
    {
        fprintf(out, "0x%lX, ", instrBin[i]);
    }

}

uint8_t parse_Cflag(Str10 tok)
{

    for (int i = 0; i < 6; i++) {
        if (strcmp(tok, cflag_defs[i].token) == 0) {
            return cflag_defs[i].code;
        }
    }

    printf("Unknown cFlag token: %s\n", tok);
    exit(EXIT_FAILURE);

}
uint8_t parse_reg(Str10 tok, uint8_t opType, uint8_t force_scalar)
{
    if((opType == OP_TYPE_MATRIX || opType == OP_TYPE_VEC4) && !force_scalar)
    {
        if(tok[0] != TOKEN_REG_M) 
        {
            printf("Use matrix reg %s\n", tok);
            exit(EXIT_FAILURE);
        }
        if(strcmp(tok, TOKEN_REG_M_IN) == 0)
        {
            return REG_M_IN;
        }
        uint8_t num = tok[1] - '0'; 
        if(num > REG_MAT_SIZE)
        {
            printf("Unknown reg %s\n", tok);
            exit(EXIT_FAILURE);
        }
        return num;
    }
    else 
    {
        if(strcmp(tok, TOKEN_REG_PX) == 0)
            return REG_PX;
        else if(strcmp(tok, TOKEN_REG_PY) == 0)
            return REG_PY;
        else if(strcmp(tok, TOKEN_REG_PR) == 0)
            return REG_PR;
        else if(strcmp(tok, TOKEN_REG_PG) == 0)
            return REG_PG;
        else if(strcmp(tok, TOKEN_REG_PB) == 0)
            return REG_PB;
        else 
        {
             if(tok[0] != TOKEN_REG_P) 
            {
                printf("Use pReg %s\n", tok);
                exit(EXIT_FAILURE);
            }
            uint8_t num = tok[1] - '0'; 
            if(num > REG_P_GEN_SIZE)
            {
                printf("Unknown reg %s\n", tok);
                exit(EXIT_FAILURE);
            }
            return num;
        }
    }
    printf("Unknown reg %s\n", tok);
    return 0;
}


arg_data parse_arg(Str10 tok, op_def op)
{
    arg_data arg;
    char* endptr;
    uint8_t parseScalar =   op.opcode == INSTR_TRANS  ||  
                            op.opcode == INSTR_ROTX   || 
                            op.opcode == INSTR_ROTY   || 
                            op.opcode == INSTR_LDU    ||
                            op.opType ==  OP_TYPE_F32 ||  
                            op.opType ==  OP_TYPE_U32;

    long int_val = strtol(tok, &endptr, 10);
    if(parseScalar)
    {
        if (*endptr == '\0') 
        { 
            arg.type = ARG_TYPE_IMM;
            arg.val.u32 = int_val;
            return arg;
        } 
        else 
        {
            float float_val = strtof(tok, &endptr);
            if (*endptr == '\0') 
            {
                arg.type = ARG_TYPE_IMM;
                arg.val.f32 = float_val;
                return arg;
            } 
            else 
            {
                arg.type = ARG_TYPE_REG; 
                arg.val.u32 = parse_reg(tok, op.opType, 1);
                return arg;
            }
        }
    }
    else 
    {
        arg.type = ARG_TYPE_REG; 
        arg.val.u32 = parse_reg(tok, op.opType, 0);
        return arg;
    }
    
    return arg;
}

int process_tokens(Vector *tokens)
{
    int new_token = 1;
    int isCMP = 0;
    int argc = 0;
    int parsedDest = 0;
    Instr current_instr = (Instr){};
    op_def op;
    for (size_t i = 0; i < tokens->size; i++) 
    {
        printf("Token %zu: %s\n", i, tokens->items[i]);
        if(!new_token)
        {
            if(isCMP)
            {
                current_instr.cFlag = parse_Cflag(tokens->items[i]);
                isCMP = 0;
                continue;
            }
            if(!parsedDest)
            {
                current_instr.dest = parse_reg(tokens->items[i], op.opType, 0);
                parsedDest = 1;
                if(op.argc == 0)
                {
                    parseInstr(&current_instr);
                    new_token = 1;
                }
                continue;
            }
            if(argc < op.argc)
            {
                arg_data arg = parse_arg(tokens->items[i], op);
                switch (argc)
                {
                case 0:
                    current_instr.arg0     = arg.val;
                    current_instr.arg0Type = arg.type;
                    break;
                case 1:
                    current_instr.arg1     = arg.val;
                    current_instr.arg1Type = arg.type;
                    break;
                case 2:
                    current_instr.arg2     = arg.val;
                    current_instr.arg2Type = arg.type;
                break;
                default:
                    printf("Too many args\n");
                    exit(EXIT_FAILURE);
                    break;
                }
                argc++;
            }
            if(argc == op.argc) 
            {
                parseInstr(&current_instr);
                new_token = 1;
                continue;
            }
        }
        else
        {
            current_instr = (Instr){};
            op = new_instr(tokens->items[i]);
            current_instr.opcode = op.opcode;
            current_instr.cFlag  = op.isCflag == 1 ? C_FLAG_ENABLE : C_FLAG_DISABLE;
            current_instr.opType = op.opType;
            new_token = 0;
            parsedDest = 0;
            switch (op.opcode)
            {
            case INSTR_EXIT:
                parseInstr(&current_instr);
                new_token = 1;
                break;
            case INSTR_CMP:
                parsedDest = 1;
                break;
            break;
            }
            isCMP = op.opcode == INSTR_CMP;
            argc = 0;
            continue;
        }
          
    }
    return 0;
}
int main(int argc,char** argv)
{
    if(argc != 2)
    {
        printf("Usage: compiler <filename>\n");
        return EXIT_FAILURE;
    }
    printf("Start\n");
    Vector tokens;
    vector_init(&tokens);
    int status = tokenize_file(argv[1], &tokens);
    
    if (status != 1) {
        printf("Tokenizing failed.\n");
        return 1;
    }

    out = fopen("output.txt", "w");
    fprintf(out, "uint64_t bin_shader[] = { ");
    process_tokens(&tokens);
    if (fseek(out, -2, SEEK_END) != 0) {
        fclose(out);
        return 1;
    }
    fputs(" };\n", out);
    fclose(out);

    return EXIT_SUCCESS;
}