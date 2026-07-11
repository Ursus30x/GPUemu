#include "jit_flow.h"


void handle_op_type_function(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    // Operand[0] is the return type; the remaining operands are argument types.
    // For the GPU emulator (main), we expect a single function with a specific signature, so we can directly map this to our predefined function type.
    // TO-DO handle multiple functions and more complex signatures in the future if needed.
    ctx->type_info[res_id].opcode = SpvOpTypeFunction;
}
// static int8_t is_vs_out(const char* name)
// {
//     if(name == NULL)
//     {
//         return 0;
//     }
//     return strcmp(name, VS_OUT_NAME) == 0;
// }
void resolve_pending_globals(JitContext* ctx) 
{
    /* Use the function parameter values (passed into main_simt) instead of module globals */
    LLVMValueRef ectx_ptr = ctx->env_arg_param;

    for (uint32_t i = 0; i < ctx->global_count; i++) 
    {
        GlobalResolution* g = &ctx->globals[i];
        //char *name = ctx->names[g->base_type];
        
        uint32_t field_idx = 0;
        if (g->storage_class == SpvStorageClassUniform || g->storage_class == SpvStorageClassStorageBuffer)
            field_idx = 0; // binding_buffers
        else if (g->storage_class == SpvStorageClassInput)
            field_idx = 1; // location_in_buffers
        else if (g->storage_class == SpvStorageClassOutput)
            field_idx = 2; // location_out_buffers
        else continue;


        if(g->storage_class == SpvStorageClassOutput && g->binding_or_loc == -1)
        {
            // The "vertexOut" struct is the 4th element (index 3) in exec_ctx_fields
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0), // Pointer dereference
                LLVMConstInt(ctx->int_type, 0, 0)  // Field 0: llvm
            };

            /* vs_data_param points to the builtin vertex output struct for this invocation */
            LLVMValueRef vs_data = ctx->vs_data_param;
            /* get the ADDRESS of the vertexOut block relative to the passed-in vs_data pointer */
            LLVMValueRef vertex_out_addr = LLVMBuildInBoundsGEP2(
                ctx->builder,
                ctx->vs_data_type,
                vs_data,
                indices,
                2,
                "vertex_out_ptr"
            );
            set_val(ctx, g->res_id, vertex_out_addr);
        }
        else 
        {
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
}

void handle_op_function(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands)
{
    // operands[0]: Function Control 
    // operands[1]: Function Type ID 
    // TO-DO handle multiple functions and more complex signatures in the future if needed.

    if (ctx->func == NULL) 
    {
        // Use the pre-built ExecutionContext and vs_data struct types from init_jit
        // Function Signature: void main_simt(ExecutionContext* ectx, BuiltinVertexOutput* vs_out)
        LLVMTypeRef param_types[2];
        LLVMTypeRef ectx_ptr_type = LLVMPointerType(ctx->exec_ctx_type, 0);
        LLVMTypeRef vs_data_ptr_type = LLVMPointerType(ctx->vs_data_type, 0);
        param_types[0] = ectx_ptr_type;
        param_types[1] = vs_data_ptr_type;

        LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 2, 0);
        ctx->func = LLVMAddFunction(ctx->module, "main_simt", func_type);

        /* Capture the LLVM parameter values for use during codegen */
        ctx->env_arg_param = LLVMGetParam(ctx->func, 0);
        ctx->vs_data_param = LLVMGetParam(ctx->func, 1);

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