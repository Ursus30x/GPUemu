#include "jit_decorators.h"

void handle_op_decorate(JitContext* ctx, uint32_t* operands, int op_count) 
{
    uint32_t target_id = operands[0];
    uint32_t decoration = operands[1];
    uint32_t value = (op_count > 2) ? operands[2] : 0;

    ctx->decorations[target_id].is_decorated = 1;
    DEBUG_PRINT("Decorating ID %d with decoration %d (value: %d)\n", target_id, decoration, value);
    switch(decoration) {
        case SpvDecorationBinding:
            ctx->decorations[target_id].binding = value;
            break;
        case SpvDecorationDescriptorSet:
            ctx->decorations[target_id].descriptor_set = value;
            break;
        case SpvDecorationLocation:
            ctx->decorations[target_id].location = value;
            break;
        case SpvDecorationBuiltIn:
            ctx->decorations[target_id].builtin = value;
            break;
        default:
            DEBUG_PRINT("Ignored decoration %d on ID %d\n", decoration, target_id);
            break;
    }
}
void handle_op_type_pointer(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    //uint32_t storage_class = operands[0];
    uint32_t type_id =  operands[1];
    ctx->type_info[res_id].opcode = SpvOpTypePointer;
    ctx->type_info[res_id].base_type_id = type_id; 
}

void handle_op_type_array(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    uint32_t element_type_id = operands[0];
    //uint32_t length_id  = operands[1];
    ctx->type_info[res_id].opcode = SpvOpTypeArray;
    ctx->type_info[res_id].base_type_id = element_type_id;
}

void handle_op_type_struct(JitContext* ctx, uint32_t res_id, uint32_t* member_types, int count) 
{
    ctx->type_info[res_id].opcode = SpvOpTypeStruct;
    ctx->type_info[res_id].member_count = count;
    ctx->type_info[res_id].member_types = malloc(sizeof(uint32_t) * count);
    memcpy(ctx->type_info[res_id].member_types, member_types, sizeof(uint32_t) * count);
}

void handle_op_type_void(JitContext* ctx, uint32_t res_id)
{
    ctx->type_info[res_id].opcode = SpvOpTypeVoid;
    ctx->type_info[res_id].base_type_id = 0;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = 0;
}
void handle_op_type_bool(JitContext* ctx, uint32_t res_id)
{
    ctx->type_info[res_id].opcode = SpvOpTypeBool;
    ctx->type_info[res_id].base_type_id = 0;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = 0;
}

void handle_op_type_int(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeInt;

    uint32_t width = operands[0];
    uint32_t signedness = operands[1];

    ctx->type_info[res_id].base_type_id = width;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = signedness;
}
void handle_op_type_float(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeFloat;

    uint32_t width = operands[0];

    ctx->type_info[res_id].base_type_id = width;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = 0;
}
void handle_op_type_vector(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeVector;

    uint32_t component_type = operands[0];
    uint32_t count = operands[1];

    ctx->type_info[res_id].base_type_id = component_type;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = count;
}
void handle_op_type_matrix(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeMatrix;

    uint32_t column_type = operands[0];
    uint32_t column_count = operands[1];

    ctx->type_info[res_id].base_type_id = column_type; 
    ctx->type_info[res_id].member_types = NULL;       
    ctx->type_info[res_id].member_count = column_count;
}
void handle_op_member_decorate(JitContext* ctx, uint32_t* operands) 
{
    uint32_t struct_id = operands[0];
    uint32_t member = operands[1];
    uint32_t decoration = operands[2];
    uint32_t value = operands[3];
    MemberDecoNode* node = ctx->member_decorations[struct_id];
    while(node) 
    {
        if(node->member_index == member) break;
        node = node->next;
    }
    if(!node) 
    {
        node = malloc(sizeof(MemberDecoNode));
        node->matrix_stride = -1; // Default
        node->member_index = member;
        node->offset = 0; // Default
        node->next = ctx->member_decorations[struct_id];
        ctx->member_decorations[struct_id] = node;
    }
    
    if(decoration == SpvDecorationOffset) 
    {
        node->offset = value;
    }
}
static uint32_t spirv_string_word_length(const char* str)
{
    uint32_t len = (uint32_t)strlen(str) + 1;
    return (len + 3) / 4;
}

void handle_op_entry_point(JitContext* ctx, uint32_t* operands, uint32_t operand_count)
{
    uint32_t execution_model = operands[0];
    uint32_t entry_point_id  = operands[1];

    const char* name = (const char*)&operands[2];

    memcpy(ctx->shader_info.entry_point_name, name, sizeof(ctx->shader_info.entry_point_name));
    ctx->shader_info.execution_model = execution_model;

    DEBUG_PRINT("Entry Point: ID %u, Execution Model %u\n", entry_point_id, execution_model);
    DEBUG_PRINT("Entry Point Name: %s\n", name);

    uint32_t name_words = spirv_string_word_length(name);

    uint32_t interface_start = 2 + name_words;


    ctx->shader_info.interface_count = operand_count - interface_start;
    DEBUG_PRINT("Interface IDs:\n");

    for (uint32_t i = interface_start; i < operand_count; i++)
    {
        ctx->shader_info.interface[i - interface_start].id = operands[i];
        DEBUG_PRINT("  Interface ID: %u\n", operands[i]);
    }
}