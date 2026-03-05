#include "jit_mem.h"


//handle_op_variable: Maps SPIR-V variables to physical resources or local memory.
//operands[0]: Storage Class (Uniform, Input, Output, Function, etc.)
void handle_op_variable(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t opCount, uint32_t* operands) 
{
    uint32_t storage_class = operands[0];
    SpvDecoInfo* deco = &ctx->decorations[res_id];

    ctx->type_kind_map[res_id] = (uint8_t)storage_class;

    if (storage_class == SpvStorageClassUniform || storage_class == SpvStorageClassStorageBuffer) 
    {
        int32_t binding = deco->binding;
        if (binding >= 0 && binding < MAX_BINDINGS) 
        {
            // Path: ExecutionContext* env -> uniform_buffers[binding]
            // ExecutionContext struct layout:
            // 0: uint8_t* uniform_buffers[MAX_BINDINGS]
            // 1: uint8_t* vertex_buffers[MAX_ATTRIBUTES]
            
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),       // Dereference env_arg pointer
                LLVMConstInt(ctx->int_type, 0, 0),       // Access 'uniform_buffers' field (index 0)
                LLVMConstInt(ctx->int_type, binding, 0)  // Access specific binding index
            };

            // Calculate address of the pointer in the ExecutionContext
            LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->ptr_type, ctx->env_arg, indices, 3, "ubo_slot");
            // Load the actual buffer address (e.g., the pointer we put in exec_ctx.uniform_buffers[0])
            LLVMValueRef buffer_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "ubo_ptr");
            
            set_val(ctx, res_id, buffer_ptr);
        } 
        else 
        {
            set_val(ctx, res_id, LLVMConstNull(ctx->ptr_type));
        }
    } 
    else if (storage_class == SpvStorageClassInput)
    {
        int32_t location = deco->location;
        if (location >= 0 && location < MAX_ATTRIBUTES) {
            // Path: ExecutionContext* env -> vertex_buffers[location]
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, 1, 0),       // Access 'vertex_buffers' field (index 1)
                LLVMConstInt(ctx->int_type, location, 0)
            };

            LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->ptr_type, ctx->env_arg, indices, 3, "vtx_slot");
            LLVMValueRef buffer_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "vtx_ptr");
            
            set_val(ctx, res_id, buffer_ptr);
        } 
        else 
        {
            set_val(ctx, res_id, LLVMConstNull(ctx->ptr_type));
        }
    }
    else if (storage_class == SpvStorageClassFunction || storage_class == SpvStorageClassPrivate) 
    {
        LLVMValueRef alloca_inst = LLVMBuildAlloca(ctx->builder, ctx->vec_float_type, "local_var");
        LLVMSetAlignment(alloca_inst, 64);
        set_val(ctx, res_id, alloca_inst);

        if (opCount > 1) 
        {
            uint32_t init_id = operands[1];
            LLVMValueRef init_val = ctx->id_val_map[init_id]; // Get the constant value
            
            if (init_val != NULL) 
            {
                LLVMBuildStore(ctx->builder, init_val, alloca_inst);
            } else 
            {
                DEBUG_PRINT("Warning: Initializer %u not found for variable %u\n", init_id, res_id);
            }
        }
    }
    else 
    {
        // ALWAYS have a fallback to prevent silent NULL pointer segfaults!
        DEBUG_PRINT("Warning: Unhandled Storage Class %u\n", storage_class);
        set_val(ctx, res_id, LLVMConstNull(ctx->ptr_type));
    }
}



// handle_op_load: Reads data from memory into a register (ID).
void handle_op_load(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) 
{
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    
    uint32_t storage_class = ctx->type_kind_map[ptr_id];
    LLVMValueRef final_val;

    if (LLVMIsConstant(ptr) && LLVMIsNull(ptr)) 
    {
        DEBUG_PRINT("Warning: OpLoad from NULL pointer ID %u\n", ptr_id);
        set_val(ctx, res_id, LLVMConstNull(ctx->vec_float_type));
        return;
    }

    if (storage_class == SpvStorageClassFunction) 
    {
        final_val = LLVMBuildLoad2(ctx->builder, ctx->vec_float_type, ptr, "v_load");
        LLVMSetAlignment(final_val, 64);
    } 
    else 
    {
        LLVMValueRef scalar_val = LLVMBuildLoad2(ctx->builder, ctx->float_type, ptr, "s_load");
        final_val = LLVMGetUndef(ctx->vec_float_type);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            LLVMValueRef idx = LLVMConstInt(ctx->int_type, i, 0);
            final_val = LLVMBuildInsertElement(ctx->builder, final_val, scalar_val, idx, "v_broadcast");
        }
    }
    
    set_val(ctx, res_id, final_val);
    DEBUG_PRINT("OpLoad ID %u: Vector result from Pointer %u (Class %u)\n", res_id, ptr_id, storage_class);
}
// handle_op_store: Writes data from a register (ID) into memory.
void handle_op_store(JitContext* ctx, uint32_t* operands) 
{
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    LLVMValueRef val = get_val(ctx, operands[1]);
    
    uint32_t storage_class = ctx->type_kind_map[ptr_id];

    if (LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMVectorTypeKind) 
    {
        LLVMValueRef vec = LLVMGetUndef(ctx->vec_float_type);
        for (int i = 0; i < SIMT_WIDTH; i++)
        {
            vec = LLVMBuildInsertElement(ctx->builder, vec, val, LLVMConstInt(ctx->int_type, i, 0), "v_store_broadcast");
        }
        val = vec;
    }

    build_masked_store(ctx, val, ptr, ctx->emask);
    
    DEBUG_PRINT("OpStore Value ID %u into Pointer ID %u (Class %u)\n", operands[1], ptr_id, storage_class);
}


/**
 * handle_op_access_chain: Calculates pointer offsets using raw integer arithmetic.
 */
void handle_op_access_chain(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count) 
{
    uint32_t base_id = operands[0];
    LLVMValueRef base_ptr = get_val(ctx, base_id);
    
    if (!base_ptr) 
    {
        base_ptr = LLVMConstNull(ctx->ptr_type);
    }
    
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMValueRef ptr_as_int = LLVMBuildPtrToInt(ctx->builder, base_ptr, i64_type, "ptr_base_addr");
    LLVMValueRef total_offset = LLVMConstInt(i64_type, 0, 0);

    uint32_t current_type_id = ctx->type_info[base_id].base_type_id;

    for (int i = 1; i < operand_count; i++) 
    {
        uint32_t index_id = operands[i];
        LLVMValueRef idx_val = get_val(ctx, index_id);

        if (!idx_val) idx_val = LLVMConstInt(ctx->int_type, 0, 0);

        if (LLVMGetTypeKind(LLVMTypeOf(idx_val)) == LLVMVectorTypeKind) 
        {
            idx_val = LLVMBuildExtractElement(ctx->builder, idx_val, LLVMConstInt(ctx->int_type, 0, 0), "idx_s");
        }
        idx_val = LLVMBuildZExt(ctx->builder, idx_val, i64_type, "idx64");

        SpvTypeInfo* info = &ctx->type_info[current_type_id];

        if (info->opcode == SpvOpTypeStruct) 
        {
            uint64_t member_idx = LLVMIsAConstantInt(idx_val) ? LLVMConstIntGetZExtValue(idx_val) : 0;
            int32_t offset_bytes = 0;
            MemberDecoNode* m = ctx->member_decorations[current_type_id];
            while (m) 
            {
                if (m->member_index == (uint32_t)member_idx) {
                    offset_bytes = m->offset;
                    break;
                }
                m = m->next;
            }
            total_offset = LLVMBuildAdd(ctx->builder, total_offset, LLVMConstInt(i64_type, offset_bytes, 0), "struct_off");
            current_type_id = info->member_types[member_idx];
        } 
        else if (info->opcode == SpvOpTypeArray) 
        {
            int32_t stride = ctx->decorations[current_type_id].array_stride;
            if (stride <= 0) stride = 4; 
            LLVMValueRef array_off = LLVMBuildMul(ctx->builder, idx_val, LLVMConstInt(i64_type, stride, 0), "arr_step");
            total_offset = LLVMBuildAdd(ctx->builder, total_offset, array_off, "arr_off");
            current_type_id = info->base_type_id;
        }
    }

    LLVMValueRef final_addr = LLVMBuildAdd(ctx->builder, ptr_as_int, total_offset, "final_ptr_int");
    LLVMValueRef final_ptr = LLVMBuildIntToPtr(ctx->builder, final_addr, ctx->ptr_type, "access_chain_ptr");

    set_val(ctx, res_id, final_ptr);
    ctx->type_kind_map[res_id] = ctx->type_kind_map[base_id];
}