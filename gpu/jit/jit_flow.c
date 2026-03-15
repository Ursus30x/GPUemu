#include "jit_flow.h"


void handle_op_type_function(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    // Operand[0] is the return type; the remaining operands are argument types.
    // For the GPU emulator (main), we expect a single function with a specific signature, so we can directly map this to our predefined function type.
    // TO-DO handle multiple functions and more complex signatures in the future if needed.
    ctx->type_info[res_id].opcode = SpvOpTypeFunction;
}

void handle_op_function(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands)
{
    // operands[0]: Function Control 
    // operands[1]: Function Type ID 
    // TO-DO handle multiple functions and more complex signatures in the future if needed.

    if (ctx->func == NULL) 
    {
        // Use the pre-built ExecutionContext struct type from init_jit
        // Function Signature: void main_simt(ExecutionContext* env, float* out_ptr)
        LLVMTypeRef param_types[] = { 
            LLVMPointerType(ctx->exec_ctx_type, 0), 
            LLVMPointerType(ctx->vec_float_type, 0) 
        };
        LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 2, 0);
        ctx->func = LLVMAddFunction(ctx->module, "main_simt", func_type);
        ctx->out_ptr_arg = LLVMGetParam(ctx->func, 1);
        ctx->env_arg = LLVMGetParam(ctx->func, 0);

        LLVMAddTargetDependentFunctionAttr(ctx->func, "no-trapping-math", "true");
        LLVMAddTargetDependentFunctionAttr(ctx->func, "stack-protector-buffer-size", "8");
        LLVMAddTargetDependentFunctionAttr(ctx->func, "prefer-vector-width", "256");
    }
}

void handle_op_label(JitContext* ctx, uint32_t res_id) 
{
    char name[32];
    snprintf(name, sizeof(name), "label_%u", res_id);
    
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, name);
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    
    set_val(ctx, res_id, (LLVMValueRef)bb); 
}