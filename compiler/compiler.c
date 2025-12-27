#define _POSIX_C_SOURCE 200809L 
#define VECTOR_IMPLEMENTATION
#include "compiler.h"
#include <ctype.h>

static const TokenDef token_defs[] = {
    { TOKEN_MOVM,  INSTR_MOV,   OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_MOV,   INSTR_MOV,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_ROTX,  INSTR_ROTX,  OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_ROTY,  INSTR_ROTY,  OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_IDENT, INSTR_IDENT, OP_TYPE_MATRIX, 0, FORMAT_D },
    { TOKEN_TRANS, INSTR_TRANS, OP_TYPE_MATRIX, 3, FORMAT_D_A_B_C },
    { TOKEN_MVP,   INSTR_MVP,   OP_TYPE_MATRIX, 0, FORMAT_D },
    { TOKEN_EXIT,  INSTR_EXIT,  OP_TYPE_U32,    0, FORMAT_NONE },
    { TOKEN_CMPI,  INSTR_CMP,   OP_TYPE_U32,    2, FORMAT_CMP },
    { TOKEN_CMPF,  INSTR_CMP,   OP_TYPE_F32,    2, FORMAT_CMP },
    { TOKEN_MULM,  INSTR_MUL,   OP_TYPE_MATRIX, 2, FORMAT_D_A_B },
    { TOKEN_MULV,  INSTR_MUL,   OP_TYPE_VEC4,   2, FORMAT_D_A_B },
    { TOKEN_MULI,  INSTR_MUL,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_MULF,  INSTR_MUL,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_ADDI,  INSTR_ADD,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_ADDF,  INSTR_ADD,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_SUBI,  INSTR_SUB,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_SUBF,  INSTR_SUB,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_DIVI,  INSTR_DIV,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_DIVF,  INSTR_DIV,   OP_TYPE_F32,    2, FORMAT_D_A_B },
    { TOKEN_MOD,   INSTR_MOD,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_COL,   INSTR_COL,   OP_TYPE_U32,    3, FORMAT_D_A_B_C },
    { TOKEN_FSAN,  INSTR_FSAN,  OP_TYPE_F32,    0, FORMAT_D },
    { TOKEN_BLENDI,INSTR_BLEND, OP_TYPE_U32,    3, FORMAT_D_A_B_C },
    { TOKEN_BLENDF,INSTR_BLEND, OP_TYPE_F32,    3, FORMAT_D_A_B_C },
    { TOKEN_LERPI, INSTR_LERP,  OP_TYPE_U32,    3, FORMAT_D_A_B_C },
    { TOKEN_LERPF, INSTR_LERP,  OP_TYPE_F32,    3, FORMAT_D_A_B_C },
    { TOKEN_ABSI,  INSTR_ABS,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_ABSF,  INSTR_ABS,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_SQRT,  INSTR_SQRT,  OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_SIN,   INSTR_SIN,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_COS,   INSTR_COS,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_CASTI, INSTR_CAST,  OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_CASTF, INSTR_CAST,  OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_LDUM,  INSTR_LDU,   OP_TYPE_MATRIX, 1, FORMAT_D_A },
    { TOKEN_LDUV,  INSTR_LDU,   OP_TYPE_VEC4,   1, FORMAT_D_A },
    { TOKEN_LDUI,  INSTR_LDU,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_LDUF,  INSTR_LDU,   OP_TYPE_F32,    1, FORMAT_D_A },
    { TOKEN_JMP,   INSTR_JMP,   OP_TYPE_U32,    1, FORMAT_JMP },
    { TOKEN_AND,   INSTR_AND,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_OR,    INSTR_OR,    OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_NOT,   INSTR_NOT,   OP_TYPE_U32,    1, FORMAT_D_A },
    { TOKEN_XOR,   INSTR_XOR,   OP_TYPE_U32,    2, FORMAT_D_A_B },
    { TOKEN_PCMPI, INSTR_PCMP,  OP_TYPE_U32,    2, FORMAT_PCMP },
    { TOKEN_PCMPF, INSTR_PCMP,  OP_TYPE_F32,    2, FORMAT_PCMP }
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
    if (!fp) return 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp)) 
    {
        char *comment_ptr = strchr(buffer, ';');
        if (comment_ptr) *comment_ptr = '\0';
        char *token = strtok(buffer, " \t\r\n,");
        while (token) 
        {
            vector_push(v, my_strdup(token));
            token = strtok(NULL, " \t\r\n,");
        }
    }
    fclose(fp);
    return 1;
}

const char* peek(Assembler *as) 
{
<<<<<<< HEAD
<<<<<<< HEAD
    op_def def;
    if(tok[0] == TOKEN_C_FLAG_ENABLE)
=======
    const char* t = peek(as);
    if (t) as->cursor++;
    return t;
}

void report_error(Assembler *as, const char* msg) 
{
    as->has_error = 1;
    const char* t = peek(as);
    fprintf(stderr, "\n[ERROR] Token %zu [%s]: %s\n", as->cursor, t ? t : "EOF", msg);
    exit(EXIT_FAILURE);
}

uint8_t parse_cflag_val(Assembler *as, const char* tok) 
{
    if (!tok) report_error(as, "Missing conditional flag");
    if (strcmp(tok, TOKEN_C_FLAG_EQ) == 0) return C_FLAG_EQ;
    if (strcmp(tok, TOKEN_C_FLAG_NEQ) == 0) return C_FLAG_NEQ;
    if (strcmp(tok, TOKEN_C_FLAG_LT) == 0) return C_FLAG_LT;
    if (strcmp(tok, TOKEN_C_FLAG_GT) == 0) return C_FLAG_GT;
    if (strcmp(tok, TOKEN_C_FLAG_LTE) == 0) return C_FLAG_LTE;
    if (strcmp(tok, TOKEN_C_FLAG_GTE) == 0) return C_FLAG_GTE;
    report_error(as, "Invalid conditional flag");
    return 0;
}

void add_label(Assembler *as, const char* name, uint32_t address) 
{
    if (as->label_count >= as->label_capacity) 
>>>>>>> b96ead3 ([Compiler] Logic and pcmp instructions)
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

<<<<<<< HEAD
=======
    if (as->cursor >= as->tokens->size) return NULL;
    return (const char*)as->tokens->items[as->cursor];
>>>>>>> fbed498 ([Compiler] Better compiler arch and labels)
}

const char* consume(Assembler *as) 
{
<<<<<<< HEAD
=======
        if (def->format == FORMAT_CMP || def->format == FORMAT_PCMP) 
        {
             instr.cFlag = parse_cflag_val(as, consume(as));
        }

        // Handle destination register
        if (def->format != FORMAT_NONE && def->format != FORMAT_CMP && 
            def->format != FORMAT_A && def->format != FORMAT_JMP) 
            {
            instr.dest = parse_reg(as, consume(as), def->opType, 0);
        }
>>>>>>> b96ead3 ([Compiler] Logic and pcmp instructions)
=======
    const char* t = peek(as);
    if (t) as->cursor++;
    return t;
}
>>>>>>> fbed498 ([Compiler] Better compiler arch and labels)

void report_error(Assembler *as, const char* msg) 
{
    as->has_error = 1;
    const char* t = peek(as);
    fprintf(stderr, "\n[ERROR] Token %zu [%s]: %s\n", as->cursor, t ? t : "EOF", msg);
    exit(EXIT_FAILURE);
}

uint8_t parse_cflag_val(Assembler *as, const char* tok)
{
<<<<<<< HEAD
    if((opType == OP_TYPE_MATRIX || opType == OP_TYPE_VEC4) && !force_scalar)
    {
        if(tok[0] != TOKEN_REG_M) 
        {
            printf("Use matrix reg %s\n", tok);
            exit(EXIT_FAILURE);
        }
<<<<<<< HEAD
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
=======
        if (as->verbose) print_instr_debug(head, &instr);
    } else {
        // Pass 1: skip appropriate amount of tokens to keep PC in sync
        if (def->format == FORMAT_CMP || def->format == FORMAT_PCMP) consume(as); // condition
        if (def->format != FORMAT_NONE && def->format != FORMAT_CMP && 
            def->format != FORMAT_A && def->format != FORMAT_JMP) consume(as); // dest
        for (int i = 0; i < def->argc; i++) consume(as);
>>>>>>> b96ead3 ([Compiler] Logic and pcmp instructions)
    }
    printf("Unknown reg %s\n", tok);
=======
    if (!tok) report_error(as, "Missing conditional flag");
    if (strcmp(tok, TOKEN_C_FLAG_EQ) == 0) return C_FLAG_EQ;
    if (strcmp(tok, TOKEN_C_FLAG_NEQ) == 0) return C_FLAG_NEQ;
    if (strcmp(tok, TOKEN_C_FLAG_LT) == 0) return C_FLAG_LT;
    if (strcmp(tok, TOKEN_C_FLAG_GT) == 0) return C_FLAG_GT;
    if (strcmp(tok, TOKEN_C_FLAG_LTE) == 0) return C_FLAG_LTE;
    if (strcmp(tok, TOKEN_C_FLAG_GTE) == 0) return C_FLAG_GTE;
    report_error(as, "Invalid conditional flag");
>>>>>>> fbed498 ([Compiler] Better compiler arch and labels)
    return 0;
}

void add_label(Assembler *as, const char* name, uint32_t address) 
{
    if (as->label_count >= as->label_capacity) 
    {
        as->label_capacity = as->label_capacity == 0 ? 16 : as->label_capacity * 2;
        as->labels = realloc(as->labels, sizeof(Label) * as->label_capacity);
    }
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
    if ((opType == OP_TYPE_MATRIX || opType == OP_TYPE_VEC4) && !force_scalar) 
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
            break;
        }
    }
    
    if (!def) report_error(as, "Unknown instruction or malformed label");

    if (pass == 2) 
    {
        Instr instr = {0};
        instr.opcode = def->opcode;
        instr.opType = def->opType;
        instr.cFlag  = is_cond ? C_FLAG_ENABLE : C_FLAG_DISABLE;

        if (def->format == FORMAT_CMP || def->format == FORMAT_PCMP) 
        {
             instr.cFlag = parse_cflag_val(as, consume(as));
        }

        // Handle destination register
        if (def->format != FORMAT_NONE && def->format != FORMAT_CMP && 
            def->format != FORMAT_A && def->format != FORMAT_JMP) 
            {
            instr.dest = parse_reg(as, consume(as), def->opType, 0);
        }

        for (int i = 0; i < def->argc; i++) 
        {
            arg_data arg = parse_arg(as, consume(as), def, pass);
            if (i == 0) 
            { instr.arg0 = arg.val; instr.arg0Type = arg.type; }
            else if (i == 1) 
            { instr.arg1 = arg.val; instr.arg1Type = arg.type; }
            else if (i == 2) 
            { instr.arg2 = arg.val; instr.arg2Type = arg.type; }
        }

        uint64_t *raw = (uint64_t*)&instr;
        for (size_t i = 0; i < sizeof(Instr)/8; i++) 
        {
            fprintf(as->output, "0x%lX, ", raw[i]);
        }
        if (as->verbose) print_instr_debug(head, &instr);
    } else {
        // Pass 1: skip appropriate amount of tokens to keep PC in sync
        if (def->format == FORMAT_CMP || def->format == FORMAT_PCMP) consume(as); // condition
        if (def->format != FORMAT_NONE && def->format != FORMAT_CMP && 
            def->format != FORMAT_A && def->format != FORMAT_JMP) consume(as); // dest
        for (int i = 0; i < def->argc; i++) consume(as);
    }

    as->current_pc++;
}

void assemble(Assembler *as) 
{
    as->cursor = 0;
    as->current_pc = 0;
    while (peek(as)) process_instruction(as, 1);

    as->cursor = 0;
    as->current_pc = 0;
    fprintf(as->output, "uint64_t bin_shader[] = { ");
    while (peek(as)) process_instruction(as, 2);
    fseek(as->output, -2, SEEK_CUR);
    fprintf(as->output, " };\n");
}

int main(int argc, char** argv) 
{
    if (argc < 2) return 1;
    Vector tokens;
    vector_init(&tokens);
    if (!tokenize_file(argv[1], &tokens)) return 1;

    Assembler as = { 
        .tokens = &tokens, 
        .cursor = 0, 
        .output = fopen("output.txt", "w"), 
        .verbose = 1,
        .labels = NULL,
        .label_count = 0,
        .label_capacity = 0
    };

    if (!as.output) return 1;
    assemble(&as);
    fclose(as.output);
    return 0;
}