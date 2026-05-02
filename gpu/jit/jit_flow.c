#include "jit_flow.h"


void handle_op_type_function(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    // Operand[0] is the return type; the remaining operands are argument types.
    // For the GPU emulator (main), we expect a single function with a specific signature, so we can directly map this to our predefined function type.
    // TO-DO handle multiple functions and more complex signatures in the future if needed.
    ctx->type_info[res_id].opcode = SpvOpTypeFunction;
}

void resolve_pending_globals(JitContext* ctx) 
{
    LLVMValueRef ectx_global = LLVMGetNamedGlobal(ctx->module, "ectx");
    
    LLVMValueRef ectx_ptr = ectx_global; 

    for (uint32_t i = 0; i < ctx->global_count; i++) 
    {
        GlobalResolution* g = &ctx->globals[i];
        
        uint32_t field_idx = 0;
        if (g->storage_class == SpvStorageClassUniform || g->storage_class == SpvStorageClassStorageBuffer)
            field_idx = 0; // binding_buffers
        else if (g->storage_class == SpvStorageClassInput)
            field_idx = 1; // location_in_buffers
        else if (g->storage_class == SpvStorageClassOutput)
            field_idx = 2; // location_out_buffers
        else continue;


        LLVMValueRef indices[] = {
            LLVMConstInt(ctx->int_type, 0, 0),         
            LLVMConstInt(ctx->int_type, field_idx, 0), 
            LLVMConstInt(ctx->int_type, g->binding_or_loc, 0)
        };

        // GEP into the global struct address directly
        LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(
            ctx->builder, 
            ctx->exec_ctx_type, 
            ectx_ptr,
            indices, 
            3, 
            "res_slot"
        );

        LLVMValueRef actual_data_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "actual_ptr");
        set_val(ctx, g->res_id, actual_data_ptr);
    }
}

void handle_op_function(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands)
{
    // operands[0]: Function Control 
    // operands[1]: Function Type ID 
    // TO-DO handle multiple functions and more complex signatures in the future if needed.

    if (ctx->func == NULL) 
    {
        // Use the pre-built ExecutionContext struct type from init_jit
        // Function Signature: void main_simt()
  
        LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), NULL, 0, 0);
        ctx->func = LLVMAddFunction(ctx->module, "main_simt", func_type);

        LLVMAddTargetDependentFunctionAttr(ctx->func, "no-trapping-math", "true");
        LLVMAddTargetDependentFunctionAttr(ctx->func, "stack-protector-buffer-size", "8");
        LLVMAddTargetDependentFunctionAttr(ctx->func, "prefer-vector-width", "256");
        
        ctx->current_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "init_global");
        LLVMPositionBuilderAtEnd(ctx->builder, ctx->current_block);

        resolve_pending_globals(ctx);

    }
}

void handle_op_label(JitContext* ctx, uint32_t res_id) 
{
    char name[32];
    snprintf(name, sizeof(name), "label_%u", res_id);
    
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, name);
    if(ctx->current_block != NULL)
    {
        LLVMBuildBr(ctx->builder, bb);
    }
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_block = bb;
    set_val(ctx, res_id, (LLVMValueRef)bb); 
}