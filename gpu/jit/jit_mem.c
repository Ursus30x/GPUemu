#include "jit_mem.h"
#include "debug_gpu.h"


//handle_op_variable: Maps SPIR-V variables to physical resources or local memory.
//operands[0]: Storage Class (Uniform, Input, Output, Function, etc.)
void handle_op_variable(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) 
{
    uint32_t storage_class = operands[0];
    
    SpvDecoInfo* deco = &ctx->decorations[res_id];

   
    ctx->type_kind_map[res_id] = (uint8_t)storage_class;

    // Handle external resources (UBO / SSBO)
    if (storage_class == SpvStorageClassUniform || storage_class == SpvStorageClassStorageBuffer) 
    {
        int32_t binding = deco->binding;
        
        if (binding >= 0 && binding < MAX_BINDINGS) 
        {
            // Build GEP (GetElementPtr) path to ExecutionContext->uniform_buffers[binding]
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, binding, 0)
            };

            LLVMTypeRef env_struct_type = LLVMGetElementType(LLVMTypeOf(ctx->env_arg));
            LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(ctx->builder, env_struct_type, ctx->env_arg, indices, 3, "ubo_slot_ptr");
            LLVMValueRef buffer_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "ubo_ptr");
            
            set_val(ctx, res_id, buffer_ptr);
            DEBUG_PRINT("OpVariable ID %u: Mapped to Uniform Binding %d\n", res_id, binding);
        }
    } 
    
    // 2. Handle inputs (Attributes / Vertex Data)
    else if (storage_class == SpvStorageClassInput)
    {
        int32_t location = deco->location;
        if (location >= 0 && location < MAX_ATTRIBUTES)
        {
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, 1, 0),
                LLVMConstInt(ctx->int_type, location, 0)
            };

            LLVMTypeRef env_struct_type = LLVMGetElementType(LLVMTypeOf(ctx->env_arg));
            LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(ctx->builder, env_struct_type, ctx->env_arg, indices, 3, "vtx_slot_ptr");
            LLVMValueRef buffer_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "vtx_ptr");
            
            set_val(ctx, res_id, buffer_ptr);
            DEBUG_PRINT("OpVariable ID %u: Mapped to Input Location %d\n", res_id, location);
        }
    }
    
    // Handle local memory (Variables inside functions)
    else if (storage_class == SpvStorageClassFunction) 
    {

        LLVMValueRef alloca_inst = LLVMBuildAlloca(ctx->builder, ctx->vec_float_type, "local_var");
        LLVMSetAlignment(alloca_inst, 64);
        set_val(ctx, res_id, alloca_inst);
        DEBUG_PRINT("OpVariable ID %u: Local SIMT Alloca\n", res_id);
    }
}


// handle_op_load: Reads data from memory into a register (ID).
void handle_op_load(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) 
{
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    
    uint32_t storage_class = ctx->type_kind_map[ptr_id];

    LLVMValueRef final_val;


    LLVMTypeRef ptr_type = LLVMTypeOf(ptr);
    LLVMTypeRef element_type = LLVMGetElementType(ptr_type);
    int is_vec_ptr = (element_type && LLVMGetTypeKind(element_type) == LLVMVectorTypeKind);

    if (storage_class == SpvStorageClassFunction || is_vec_ptr) 
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


//handle_op_store: Writes data from a register (ID) into memory.
void handle_op_store(JitContext* ctx, uint32_t* operands) 
{
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    LLVMValueRef val = get_val(ctx, operands[1]);
    
    uint32_t storage_class = ctx->type_kind_map[ptr_id];

    if (LLVMGetTypeKind(LLVMTypeOf(val)) != LLVMVectorTypeKind) {
        LLVMValueRef vec = LLVMGetUndef(ctx->vec_float_type);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            vec = LLVMBuildInsertElement(ctx->builder, vec, val, LLVMConstInt(ctx->int_type, i, 0), "v_store_broadcast");
        }
        val = vec;
    }

    build_masked_store(ctx, val, ptr, ctx->emask);
    
    DEBUG_PRINT("OpStore Value ID %u into Pointer ID %u (Class %u)\n", operands[1], ptr_id, storage_class);
}