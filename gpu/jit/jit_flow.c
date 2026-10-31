#include "jit_flow.h"
#include <stdio.h>
#include <string.h>

static int jit_block_has_terminator(LLVMBasicBlockRef bb)
{
    return bb != NULL && LLVMGetBasicBlockTerminator(bb) != NULL;
}

static LLVMBasicBlockRef jit_get_or_create_label_block(JitContext* ctx, uint32_t label_id)
{
    LLVMValueRef existing = get_val(ctx, label_id);
    if (existing)
        return (LLVMBasicBlockRef)existing;

    char name[32];
    snprintf(name, sizeof(name), "label_%u", label_id);

    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, name);
    set_val(ctx, label_id, (LLVMValueRef)bb);
    return bb;


}

LLVMValueRef jit_get_emask(JitContext* ctx)
{
    if (ctx->emask != NULL)
        return ctx->emask;

    // Fallback: Default to all-true SIMT active mask (<16 x i1> true)
    LLVMTypeRef i1_type = LLVMInt1TypeInContext(ctx->context);
    LLVMTypeRef mask_type = LLVMVectorType(i1_type, 16);
    return LLVMConstAllOnes(mask_type);


}

static LLVMValueRef jit_to_branch_condition(JitContext* ctx, LLVMValueRef cond)
{
    if (!cond)
        return NULL;

    LLVMTypeRef cond_type = LLVMTypeOf(cond);
    if (!cond_type)
        return cond;

    LLVMTypeKind kind = LLVMGetTypeKind(cond_type);

    if (kind == LLVMVectorTypeKind)
    {
        unsigned int num_elements = LLVMGetVectorSize(cond_type);
        if (num_elements == 0)
            return cond;

        LLVMValueRef result = NULL;
        for (unsigned int i = 0; i < num_elements; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractElement(
                ctx->builder, cond, LLVMConstInt(ctx->int_type, i, 0), "lane_val");

            LLVMTypeRef elem_type = LLVMTypeOf(elem);
            LLVMTypeKind elem_kind = LLVMGetTypeKind(elem_type);
            LLVMValueRef elem_bool = elem;

            if (elem_kind == LLVMIntegerTypeKind)
            {
                if (LLVMGetIntTypeWidth(elem_type) != 1)
                {
                    elem_bool = LLVMBuildICmp(
                        ctx->builder,
                        LLVMIntNE,
                        elem,
                        LLVMConstInt(elem_type, 0, 0),
                        "lane_cmp");
                }
            }
            else if (elem_kind == LLVMFloatTypeKind)
            {
                elem_bool = LLVMBuildFCmp(
                    ctx->builder,
                    LLVMRealONE,
                    elem,
                    LLVMConstReal(elem_type, 0.0),
                    "lane_cmp");
            }

            if (result == NULL)
                result = elem_bool;
            else
                result = LLVMBuildOr(ctx->builder, result, elem_bool, "branch_cond_any");
        }

        return result;
    }

    if (kind == LLVMIntegerTypeKind)
    {
        if (LLVMGetIntTypeWidth(cond_type) == 1)
            return cond;

        return LLVMBuildICmp(
            ctx->builder,
            LLVMIntNE,
            cond,
            LLVMConstInt(cond_type, 0, 0),
            "branch_cond_cmp");
    }

    if (kind == LLVMFloatTypeKind)
    {
        return LLVMBuildFCmp(
            ctx->builder,
            LLVMRealONE,
            cond,
            LLVMConstReal(cond_type, 0.0),
            "branch_cond_cmp");
    }
    return cond;
}

void handle_op_type_function(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    (void)operands;
    ctx->type_info[res_id].opcode = SpvOpTypeFunction;
}

void handle_op_selection_merge(JitContext* ctx, uint32_t* operands)
{
    if (ctx->control_stack_depth >= MAX_CONTROL_STACK)
        return;

    JitControlConstruct* construct = &ctx->control_stack[ctx->control_stack_depth++];
    memset(construct, 0, sizeof(*construct));
    construct->kind = JIT_CFG_SELECTION;
    construct->merge_id = operands[0];
    construct->continue_id = 0;
    construct->header_id = 0;


}

void handle_op_loop_merge(JitContext* ctx, uint32_t* operands)
{
    if (ctx->control_stack_depth >= MAX_CONTROL_STACK)
        return;

    JitControlConstruct* construct = &ctx->control_stack[ctx->control_stack_depth++];
    memset(construct, 0, sizeof(*construct));
    construct->kind = JIT_CFG_LOOP;
    construct->merge_id = operands[0];
    construct->continue_id = operands[1];
    construct->header_id = 0;


}

void handle_op_branch(JitContext* ctx, uint32_t* operands)
{
    if (!ctx->func)
    return;

    uint32_t target_id = operands[0];

    if (ctx->current_block != NULL && !jit_block_has_terminator(ctx->current_block))
    {
        // Intercept block jumps within selection constructs to enable linear SIMT execution
        if (ctx->control_stack_depth > 0 && 
            ctx->control_stack[ctx->control_stack_depth - 1].kind == JIT_CFG_SELECTION)
        {
            JitControlConstruct* construct = &ctx->control_stack[ctx->control_stack_depth - 1];

            if (target_id == construct->merge_id)
            {
                // End of 'then' block reached: transition to 'else' block if present
                if (!construct->executed_true && construct->false_id != 0 && construct->false_id != construct->merge_id)
                {
                    construct->executed_true = true;

                    // Calculate active mask for 'else' branch: parent_mask & (~cond)
                    LLVMValueRef not_cond = LLVMBuildNot(ctx->builder, construct->cond, "cond_not");
                    LLVMValueRef false_mask = LLVMBuildAnd(ctx->builder, construct->parent_mask, not_cond, "simt_mask_else");
                    ctx->emask = false_mask;

                    // Divert execution into 'else' block instead of skipping to merge block
                    LLVMBasicBlockRef false_block = jit_get_or_create_label_block(ctx, construct->false_id);
                    LLVMBuildBr(ctx->builder, false_block);
                    return;
                }
                else
                {
                    // End of selection construct: restore parent active mask and pop construct
                    ctx->emask = construct->parent_mask;
                    ctx->control_stack_depth--;
                }
            }
        }

        LLVMBasicBlockRef target = jit_get_or_create_label_block(ctx, target_id);
        LLVMBuildBr(ctx->builder, target);
    }
}

static LLVMValueRef jit_to_simt_mask(JitContext* ctx, LLVMValueRef cond)
{
    if (!cond) return jit_get_emask(ctx);
    LLVMTypeRef type = LLVMTypeOf(cond);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind)
        return cond;
    if (LLVMGetTypeKind(type) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(type) != 1) {
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond, LLVMConstInt(type, 0, 0), "cond_i1");
    }
    if (LLVMIsConstant(cond)) {
        LLVMValueRef vals[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) vals[i] = cond;
        return LLVMConstVector(vals, SIMT_WIDTH);
    }
    LLVMValueRef vec = LLVMGetUndef(ctx->vec_i1_type);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        vec = LLVMBuildInsertElement(ctx->builder, vec, cond, LLVMConstInt(ctx->int_type, i, 0), "mask_splat");
    }
    return vec;
}

void handle_op_branch_conditional(JitContext* ctx, uint32_t* operands)
{
    if (!ctx->func)
        return;

    LLVMValueRef cond = get_val(ctx, operands[0]);
    uint32_t true_id = operands[1];
    uint32_t false_id = operands[2];

    LLVMBasicBlockRef true_block = jit_get_or_create_label_block(ctx, true_id);

    if (ctx->current_block != NULL && !jit_block_has_terminator(ctx->current_block))
    {
        // Check if conditional branch belongs to an OpSelectionMerge construct
        if (ctx->control_stack_depth > 0 && 
            ctx->control_stack[ctx->control_stack_depth - 1].kind == JIT_CFG_SELECTION)
        {
            JitControlConstruct* construct = &ctx->control_stack[ctx->control_stack_depth - 1];
            LLVMValueRef cond_mask = jit_to_simt_mask(ctx, cond);
            construct->true_id = true_id;
            construct->false_id = false_id;
            construct->cond = cond_mask;
            construct->parent_mask = jit_get_emask(ctx);

            // Calculate active mask for 'then' branch: parent_mask & cond
            LLVMValueRef true_mask = LLVMBuildAnd(ctx->builder, construct->parent_mask, cond_mask, "simt_mask_then");
            ctx->emask = true_mask;

            // Enter 'then' block unconditionally to process active lanes
            LLVMBuildBr(ctx->builder, true_block);
        }
        else
        {
            // Standard loop / scalar branch fallback
            LLVMBasicBlockRef false_block = jit_get_or_create_label_block(ctx, false_id);
            LLVMValueRef cond_scalar = jit_to_branch_condition(ctx, cond);
            LLVMBuildCondBr(ctx->builder, cond_scalar, true_block, false_block);
        }
    }
}

void handle_op_phi(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count)
{
    if (!ctx->func || !ctx->current_block || operand_count < 2)
        return;

    LLVMTypeRef phi_type = map_spv_to_llvm_type(ctx, type_id);
    if (!phi_type)
        phi_type = LLVMTypeOf(get_val(ctx, operands[0]));

    LLVMValueRef phi = LLVMBuildPhi(ctx->builder, phi_type, "phi");
    set_val(ctx, res_id, phi);

    uint32_t incoming_count = operand_count / 2;
    LLVMValueRef incoming_values[16];
    LLVMBasicBlockRef incoming_blocks[16];

    for (uint32_t i = 0; i < incoming_count && i < 16; ++i)
    {
        incoming_values[i] = get_val(ctx, operands[i * 2]);
        incoming_blocks[i] = jit_get_or_create_label_block(ctx, operands[i * 2 + 1]);
    }

    if (incoming_count > 0)
        LLVMAddIncoming(phi, incoming_values, incoming_blocks, incoming_count);
}

void resolve_pending_globals(JitContext* ctx)
{
    LLVMValueRef ectx_ptr = ctx->env_arg_param;

    for (uint32_t i = 0; i < ctx->global_count; i++) 
    {
        GlobalResolution* g = &ctx->globals[i];
        
        uint32_t field_idx = 0;
        if (g->storage_class == SpvStorageClassWorkgroup)
        {
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, 3, 0)
            };
            LLVMValueRef shmem_slot = LLVMBuildInBoundsGEP2(ctx->builder, ctx->exec_ctx_type, ectx_ptr, indices, 2, "shmem_slot");
            LLVMValueRef shmem_base = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, shmem_slot, "shmem_base");
            LLVMValueRef offset_val = LLVMConstInt(LLVMInt64TypeInContext(ctx->context), g->binding_or_loc, 0);
            LLVMValueRef var_ptr = LLVMBuildGEP2(ctx->builder, LLVMInt8TypeInContext(ctx->context), shmem_base, &offset_val, 1, "workgroup_var_ptr");
            set_val(ctx, g->res_id, var_ptr);
            continue;
        }
        else if (g->storage_class == SpvStorageClassUniform || g->storage_class == SpvStorageClassStorageBuffer ||  g->storage_class ==  SpvStorageClassUniformConstant)
            field_idx = 0;
        else if (g->storage_class == SpvStorageClassInput)
            field_idx = 1;
        else if (g->storage_class == SpvStorageClassOutput)
            field_idx = 2;
        else continue;

        if((g->storage_class == SpvStorageClassOutput || g->storage_class == SpvStorageClassInput) && g->binding_or_loc == -1)
        {
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, 0, 0)
            };

            if(ctx->shader_type == VERTEX_SHADER)
            {
                LLVMValueRef vs_data = ctx->vs_data_param;
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
            else if(ctx->shader_type == FRAGMENT_SHADER)
            {
                LLVMValueRef fs_data = ctx->fs_data_param;
                LLVMValueRef fs_in_addr = LLVMBuildInBoundsGEP2(
                    ctx->builder,
                    ctx->fs_data_type,
                    fs_data,
                    indices,
                    2,
                    "fs_in_ptr"
                );
                set_val(ctx, g->res_id, fs_in_addr);
            }
            else if(ctx->shader_type == COMPUTE_SHADER)
            {
                SpvDecoInfo* d = &ctx->decorations[g->res_id];
                int32_t builtin = d->builtin;
                uint32_t cs_field_idx = 0;
                if (builtin == SpvBuiltInGlobalInvocationId) cs_field_idx = 0;
                else if (builtin == SpvBuiltInLocalInvocationId) cs_field_idx = 1;
                else if (builtin == SpvBuiltInLocalInvocationIndex) cs_field_idx = 2;
                else if (builtin == SpvBuiltInWorkgroupId) cs_field_idx = 3;
                else if (builtin == SpvBuiltInNumWorkgroups) cs_field_idx = 4;
                else if (builtin == SpvBuiltInWorkgroupSize) cs_field_idx = 5;
                else if (builtin == SpvBuiltInSubgroupSize || builtin == SpvBuiltInSubgroupMaxSize) cs_field_idx = 6;
                else if (builtin == SpvBuiltInSubgroupLocalInvocationId) cs_field_idx = 7;
                else if (builtin == SpvBuiltInNumSubgroups || builtin == SpvBuiltInNumEnqueuedSubgroups) cs_field_idx = 8;
                else if (builtin == SpvBuiltInSubgroupId) cs_field_idx = 9;
                else if (builtin == SpvBuiltInSubgroupEqMask) cs_field_idx = 10;
                else if (builtin == SpvBuiltInSubgroupGeMask) cs_field_idx = 11;
                else if (builtin == SpvBuiltInSubgroupGtMask) cs_field_idx = 12;
                else if (builtin == SpvBuiltInSubgroupLeMask) cs_field_idx = 13;
                else if (builtin == SpvBuiltInSubgroupLtMask) cs_field_idx = 14;

                LLVMValueRef cs_indices[] = {
                    LLVMConstInt(ctx->int_type, 0, 0),
                    LLVMConstInt(ctx->int_type, cs_field_idx, 0)
                };
                LLVMValueRef cs_data = ctx->cs_data_param;
                LLVMValueRef cs_in_addr = LLVMBuildInBoundsGEP2(
                    ctx->builder,
                    ctx->cs_data_type,
                    cs_data,
                    cs_indices,
                    2,
                    "cs_in_ptr"
                );
                set_val(ctx, g->res_id, cs_in_addr);
            }
        }
        else 
        {
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),         
                LLVMConstInt(ctx->int_type, field_idx, 0), 
                LLVMConstInt(ctx->int_type, g->binding_or_loc, 0)
            };
            
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
    (void)res_id;
    (void)type_id;
    (void)operands;

    if (ctx->func == NULL)
    {
        LLVMTypeRef param_types[4];
        LLVMTypeRef ectx_ptr_type = LLVMPointerType(ctx->exec_ctx_type, 0);
        LLVMTypeRef vs_data_ptr_type = LLVMPointerType(ctx->vs_data_type, 0);
        LLVMTypeRef fs_data_ptr_type = LLVMPointerType(ctx->fs_data_type, 0);
        LLVMTypeRef cs_data_ptr_type = LLVMPointerType(ctx->cs_data_type, 0);

        param_types[0] = ectx_ptr_type;
        param_types[1] = vs_data_ptr_type;
        param_types[2] = fs_data_ptr_type;
        param_types[3] = cs_data_ptr_type;

        LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 4, 0);
        ctx->func = LLVMAddFunction(ctx->module, "main_simt", func_type);

        ctx->env_arg_param = LLVMGetParam(ctx->func, 0);
        ctx->vs_data_param = LLVMGetParam(ctx->func, 1);
        ctx->fs_data_param = LLVMGetParam(ctx->func, 2);
        ctx->cs_data_param = LLVMGetParam(ctx->func, 3);

        LLVMAddTargetDependentFunctionAttr(ctx->func, "no-trapping-math", "true");
        LLVMAddTargetDependentFunctionAttr(ctx->func, "stack-protector-buffer-size", "8");
        LLVMAddTargetDependentFunctionAttr(ctx->func, "prefer-vector-width", "256");
        
        ctx->current_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "init_global");
        LLVMPositionBuilderAtEnd(ctx->builder, ctx->current_block);

        LLVMValueRef mask_indices[] = {
            LLVMConstInt(ctx->int_type, 0, 0),
            LLVMConstInt(ctx->int_type, 6, 0)
        };
        LLVMValueRef mask_slot = LLVMBuildInBoundsGEP2(
            ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param,
            mask_indices, 2, "active_mask_slot");
        LLVMValueRef active_mask = LLVMBuildLoad2(
            ctx->builder, ctx->int_type, mask_slot, "active_mask");

        ctx->emask = LLVMGetUndef(ctx->vec_i1_type);
        for (int lane = 0; lane < SIMT_WIDTH; lane++) 
        {
            LLVMValueRef shifted = LLVMBuildLShr(
                ctx->builder, active_mask,
                LLVMConstInt(ctx->int_type, lane, 0), "active_lane_shift");
            LLVMValueRef bit = LLVMBuildAnd(
                ctx->builder, shifted,
                LLVMConstInt(ctx->int_type, 1, 0), "active_lane_bit");
            LLVMValueRef active = LLVMBuildICmp(
                ctx->builder, LLVMIntNE, bit,
                LLVMConstInt(ctx->int_type, 0, 0), "active_lane");
            ctx->emask = LLVMBuildInsertElement(
                ctx->builder, ctx->emask, active,
                LLVMConstInt(ctx->int_type, lane, 0), "active_mask_insert");
        }

        resolve_pending_globals(ctx);
    }
}

void handle_op_label(JitContext* ctx, uint32_t res_id)
{
    LLVMBasicBlockRef bb = jit_get_or_create_label_block(ctx, res_id);
    if(ctx->current_block != NULL && ctx->current_block != bb && !jit_block_has_terminator(ctx->current_block))
    {
        LLVMBuildBr(ctx->builder, bb);
    }
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_block = bb;
    set_val(ctx, res_id, (LLVMValueRef)bb);
}