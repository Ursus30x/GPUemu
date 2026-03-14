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
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, 1, 0),     
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
        if (opCount > 1) 
        {
            uint32_t init_id = operands[1];
            LLVMValueRef init_val = ctx->id_val_map[init_id]; 
            LLVMTypeRef type = LLVMTypeOf(init_val);
            LLVMValueRef alloca_inst = LLVMBuildAlloca(ctx->builder, type, "local_var");
            LLVMSetAlignment(alloca_inst, 64);
            set_val(ctx, res_id, alloca_inst);
            if (init_val != NULL) 
            {
                LLVMBuildStore(ctx->builder, init_val, alloca_inst);
            } 
            else 
            {
                DEBUG_PRINT("Warning: Initializer %u not found for variable %u\n", init_id, res_id);
            }
        }
        else 
        {
            LLVMTypeRef type = map_spv_to_llvm_type(ctx, type_id);
            LLVMValueRef alloca_inst = LLVMBuildAlloca(ctx->builder,type, "local_var");
            LLVMSetAlignment(alloca_inst, 64);
            set_val(ctx, res_id, alloca_inst);
        }
    }
    else 
    {
        DEBUG_PRINT("Warning: Unhandled Storage Class %u\n", storage_class);
        set_val(ctx, res_id, LLVMConstNull(ctx->ptr_type));
    }
}


static LLVMValueRef build_recursive_load(JitContext *ctx, LLVMTypeRef type, LLVMValueRef ptr) 
{
   
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind) 
    {
        LLVMValueRef load = LLVMBuildLoad2(ctx->builder, type, ptr, "load_simt");
        LLVMSetAlignment(load, 64);
        return load;
    } 
    else if (LLVMGetTypeKind(type) == LLVMArrayTypeKind) 
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        
        LLVMValueRef aggregate = LLVMGetUndef(type);

        for (uint32_t i = 0; i < count; i++) {
            LLVMValueRef index = LLVMConstInt(LLVMInt32Type(), i, 0);
            
            LLVMValueRef element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, ptr, &index, 1, "load_gep");
            
            LLVMValueRef element_val = build_recursive_load(ctx, elem_type, element_ptr);
            
            aggregate = LLVMBuildInsertValue(ctx->builder, aggregate, element_val, i, "insert_elem");
        }
        return aggregate;
    }
    return LLVMGetUndef(type);
}

void handle_op_load(JitContext* ctx, uint32_t res_id, uint32_t res_type_id, uint32_t* operands) 
{
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    
    LLVMTypeRef result_llvm_type = map_spv_to_llvm_type(ctx, res_type_id);

    if (LLVMIsConstant(ptr) && LLVMIsNull(ptr)) 
    {
        set_val(ctx, res_id, LLVMConstNull(result_llvm_type));
        return;
    }

    LLVMValueRef final_val = build_recursive_load(ctx, result_llvm_type, ptr);
    
    set_val(ctx, res_id, final_val);
}


static void build_recursive_store(JitContext *ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask) {
    LLVMTypeRef type = LLVMTypeOf(val_to_store);
    LLVMTypeKind kind = LLVMGetTypeKind(type);

    if (kind == LLVMVectorTypeKind) 
    {
        LLVMValueRef current_mem_val = LLVMBuildLoad2(ctx->builder, type, ptr, "current_mem");
        
        LLVMValueRef masked_val = LLVMBuildSelect(ctx->builder, mask, val_to_store, current_mem_val, "masked_val");
        
        LLVMValueRef store_inst = LLVMBuildStore(ctx->builder, masked_val, ptr);
        LLVMSetAlignment(store_inst, 64);
    } 
    else if (kind == LLVMArrayTypeKind) 
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMTypeRef elem_type = LLVMGetElementType(type);

        for (uint32_t i = 0; i < count; i++) 
        {
            LLVMValueRef index = LLVMConstInt(LLVMInt32Type(), i, 0);
            
            LLVMValueRef element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, ptr, &index, 1, "store_gep");
            
            LLVMValueRef element_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "extract_elem");

            build_recursive_store(ctx, element_val, element_ptr, mask);
        }
    }
}

// handle_op_store: Writes data from a register (ID) into memory.
void handle_op_store(JitContext* ctx, uint32_t* operands) {
    uint32_t ptr_id = operands[0];
    uint32_t val_id = operands[1];

    LLVMValueRef ptr = get_val(ctx, ptr_id);
    LLVMValueRef val = get_val(ctx, val_id);

    build_recursive_store(ctx, val, ptr, ctx->emask);
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