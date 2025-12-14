#define VECTOR_IMPLEMENTATION
#include "compiler.h"
FILE *out; 
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
   
    int found = 0;
    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tok, token_defs[i].token) == 0) {
            def.opcode = token_defs[i].opcode;
            def.opType = token_defs[i].opType;
            def.argc = token_defs[i].argc;
            found = 1;
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