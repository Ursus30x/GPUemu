#include "jit_atomic.h"

static bool is_float_type(LLVMTypeRef type)
{
    if (!type) return false;
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind)
        return true;
    if (kind == LLVMVectorTypeKind) {
        LLVMTypeKind elem_kind = LLVMGetTypeKind(LLVMGetElementType(type));
        return elem_kind == LLVMFloatTypeKind || elem_kind == LLVMDoubleTypeKind;
    }
    return false;
}

LLVMValueRef jit_to_int_vector(JitContext* ctx, LLVMValueRef val) 
{
    if (!val) return LLVMConstNull(LLVMVectorType(ctx->int_type, SIMT_WIDTH));
    LLVMTypeRef type = LLVMTypeOf(val);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind) 
    {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMFloatTypeKind || LLVMGetTypeKind(elem_type) == LLVMDoubleTypeKind) 
        {
            return LLVMBuildBitCast(ctx->builder, val, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "f2i_bc");
        }
        return val;
    }
    if (LLVMGetTypeKind(type) == LLVMFloatTypeKind || LLVMGetTypeKind(type) == LLVMDoubleTypeKind) 
    {
        val = LLVMBuildBitCast(ctx->builder, val, ctx->int_type, "s_f2i_bc");
    }
    LLVMValueRef vec = LLVMGetUndef(LLVMVectorType(ctx->int_type, SIMT_WIDTH));
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        vec = LLVMBuildInsertElement(ctx->builder, vec, val, LLVMConstInt(ctx->int_type, i, 0), "splat_i");
    }
    return vec;
}
LLVMValueRef jit_to_numeric_int_vector(JitContext* ctx, LLVMValueRef val) 
{
    if (!val) return LLVMConstNull(LLVMVectorType(ctx->int_type, SIMT_WIDTH));
    LLVMTypeRef type = LLVMTypeOf(val);
    LLVMTypeRef int_vec_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind) 
    {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMFloatTypeKind ||
            LLVMGetTypeKind(elem_type) == LLVMDoubleTypeKind) 
        {
            return LLVMBuildFPToSI(ctx->builder, val, int_vec_type, "numeric_f2i");
        }
        return val;
    }

    if (LLVMGetTypeKind(type) == LLVMFloatTypeKind ||
        LLVMGetTypeKind(type) == LLVMDoubleTypeKind) 
    {
        val = LLVMBuildFPToSI(ctx->builder, val, ctx->int_type, "numeric_s_f2i");
    }

    LLVMValueRef vec = LLVMGetUndef(int_vec_type);
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        vec = LLVMBuildInsertElement(ctx->builder, vec, val,
                                     LLVMConstInt(ctx->int_type, i, 0), "numeric_splat_i");
    }
    return vec;
}

LLVMValueRef jit_to_float_vector(JitContext* ctx, LLVMValueRef val) 
{
    if (!val) return LLVMConstNull(ctx->vec_float_type);
    LLVMTypeRef type = LLVMTypeOf(val);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind) 
    {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMIntegerTypeKind) 
        {
            return LLVMBuildBitCast(ctx->builder, val, ctx->vec_float_type, "i2f_bc");
        }
        return val;
    }
    if (LLVMGetTypeKind(type) == LLVMIntegerTypeKind) 
    {
        val = LLVMBuildBitCast(ctx->builder, val, ctx->float_type, "s_i2f_bc");
    }
    LLVMValueRef vec = LLVMGetUndef(ctx->vec_float_type);
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        vec = LLVMBuildInsertElement(ctx->builder, vec, val, LLVMConstInt(ctx->int_type, i, 0), "splat_f");
    }
    return vec;
}
void handle_op_atomic(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count)
{
    (void)type_id;
    (void)operand_count;
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    if (!ptr) 
    {
        ptr = LLVMConstNull(ctx->ptr_type);
    }
    LLVMTypeRef i32_ptr_type = LLVMPointerType(ctx->int_type, 0);
    LLVMValueRef ptr_i32 = LLVMBuildBitCast(ctx->builder, ptr, i32_ptr_type, "atomic_ptr_i32");

    LLVMTypeRef vec_i32_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);
    LLVMValueRef res_vec = LLVMGetUndef(vec_i32_type);

    if (opcode == SpvOpAtomicLoad)
    {
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            LLVMValueRef load_val = LLVMBuildLoad2(ctx->builder, ctx->int_type, ptr_i32, "atomic_load_val");
            LLVMSetOrdering(load_val, LLVMAtomicOrderingSequentiallyConsistent);
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, load_val, LLVMConstInt(ctx->int_type, lane, 0), "ins_at_res");
        }
        set_val(ctx, res_id, jit_to_float_vector(ctx, res_vec));
        return;
    }

    if (opcode == SpvOpAtomicStore)
    {
        uint32_t val_id = operands[3];
        LLVMValueRef val_vec = jit_to_int_vector(ctx, get_val(ctx, val_id));
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            LLVMValueRef mask_lane = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, lane, 0), "m_lane");
            LLVMBasicBlockRef store_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_store_bb");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_next_bb");

            LLVMBuildCondBr(ctx->builder, mask_lane, store_bb, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, store_bb);
            LLVMValueRef val_lane = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_val_lane");
            LLVMValueRef store_inst = LLVMBuildStore(ctx->builder, val_lane, ptr_i32);
            LLVMSetOrdering(store_inst, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMBuildBr(ctx->builder, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            ctx->current_block = next_bb;
        }
        return;
    }

    if (opcode == SpvOpAtomicCompareExchange || opcode == SpvOpAtomicCompareExchangeWeak)
    {
        uint32_t val_id = operands[4];
        uint32_t cmp_id = operands[5];
        LLVMValueRef val_vec = jit_to_int_vector(ctx, get_val(ctx, val_id));
        LLVMValueRef cmp_vec = jit_to_int_vector(ctx, get_val(ctx, cmp_id));

        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            LLVMValueRef mask_lane = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, lane, 0), "m_lane");
            LLVMBasicBlockRef cur_bb = ctx->current_block;
            LLVMBasicBlockRef cmpxchg_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_cmpxchg_bb");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_next_bb");

            LLVMBuildCondBr(ctx->builder, mask_lane, cmpxchg_bb, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, cmpxchg_bb);
            LLVMValueRef val_lane = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_val_lane");
            LLVMValueRef cmp_lane = LLVMBuildExtractElement(ctx->builder, cmp_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_cmp_lane");

            LLVMValueRef pair = LLVMBuildAtomicCmpXchg(ctx->builder, ptr_i32, cmp_lane, val_lane,
                                                       LLVMAtomicOrderingSequentiallyConsistent,
                                                       LLVMAtomicOrderingSequentiallyConsistent, false);
            LLVMValueRef old_val = LLVMBuildExtractValue(ctx->builder, pair, 0, "cmpxchg_old");
            LLVMBuildBr(ctx->builder, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            ctx->current_block = next_bb;

            LLVMValueRef phi = LLVMBuildPhi(ctx->builder, ctx->int_type, "cmpxchg_phi");
            LLVMValueRef phi_vals[2] = { LLVMConstInt(ctx->int_type, 0, 0), old_val };
            LLVMBasicBlockRef phi_bbs[2] = { cur_bb, cmpxchg_bb };
            LLVMAddIncoming(phi, phi_vals, phi_bbs, 2);

            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, phi, LLVMConstInt(ctx->int_type, lane, 0), "ins_at_res");
        }
        set_val(ctx, res_id, jit_to_float_vector(ctx, res_vec));
        return;
    }

    LLVMAtomicRMWBinOp rmw_op = LLVMAtomicRMWBinOpAdd;
    LLVMValueRef val_vec = NULL;

    if (opcode == SpvOpAtomicIIncrement) 
    {
        rmw_op = LLVMAtomicRMWBinOpAdd;
        LLVMValueRef ones[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) ones[i] = LLVMConstInt(ctx->int_type, 1, 0);
        val_vec = LLVMConstVector(ones, SIMT_WIDTH);
    } 
    else if (opcode == SpvOpAtomicIDecrement) 
    {
        rmw_op = LLVMAtomicRMWBinOpSub;
        LLVMValueRef ones[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) ones[i] = LLVMConstInt(ctx->int_type, 1, 0);
        val_vec = LLVMConstVector(ones, SIMT_WIDTH);
    } 
    else 
    {
        uint32_t val_id = operands[3];
        val_vec = jit_to_int_vector(ctx, get_val(ctx, val_id));
        switch (opcode) 
        {
            case SpvOpAtomicIAdd: rmw_op = LLVMAtomicRMWBinOpAdd; break;
            case SpvOpAtomicISub: rmw_op = LLVMAtomicRMWBinOpSub; break;
            case SpvOpAtomicSMin: rmw_op = LLVMAtomicRMWBinOpMin; break;
            case SpvOpAtomicUMin: rmw_op = LLVMAtomicRMWBinOpUMin; break;
            case SpvOpAtomicSMax: rmw_op = LLVMAtomicRMWBinOpMax; break;
            case SpvOpAtomicUMax: rmw_op = LLVMAtomicRMWBinOpUMax; break;
            case SpvOpAtomicAnd:  rmw_op = LLVMAtomicRMWBinOpAnd; break;
            case SpvOpAtomicOr:   rmw_op = LLVMAtomicRMWBinOpOr; break;
            case SpvOpAtomicXor:  rmw_op = LLVMAtomicRMWBinOpXor; break;
            case SpvOpAtomicExchange: rmw_op = LLVMAtomicRMWBinOpXchg; break;
            default: rmw_op = LLVMAtomicRMWBinOpAdd; break;
        }
    }

    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        LLVMValueRef mask_lane = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, lane, 0), "m_lane");
        LLVMBasicBlockRef cur_bb = ctx->current_block;
        LLVMBasicBlockRef rmw_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_rmw_bb");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_next_bb");

        LLVMBuildCondBr(ctx->builder, mask_lane, rmw_bb, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, rmw_bb);
        LLVMValueRef val_lane = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_val_lane");
        LLVMValueRef old_val = LLVMBuildAtomicRMW(ctx->builder, rmw_op, ptr_i32, val_lane, LLVMAtomicOrderingSequentiallyConsistent, false);
        LLVMBuildBr(ctx->builder, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        ctx->current_block = next_bb;

        LLVMValueRef phi = LLVMBuildPhi(ctx->builder, ctx->int_type, "rmw_phi");
        LLVMValueRef phi_vals[2] = { LLVMConstInt(ctx->int_type, 0, 0), old_val };
        LLVMBasicBlockRef phi_bbs[2] = { cur_bb, rmw_bb };
        LLVMAddIncoming(phi, phi_vals, phi_bbs, 2);

        res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, phi, LLVMConstInt(ctx->int_type, lane, 0), "ins_at_res");
    }
    set_val(ctx, res_id, jit_to_float_vector(ctx, res_vec));
}

void handle_op_group_non_uniform(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count)
{
    (void)type_id;
    (void)operand_count;

    if (opcode == SpvOpGroupNonUniformElect)
    {
        LLVMValueRef is_first_vec = LLVMGetUndef(ctx->vec_i1_type);
        LLVMValueRef prev_any = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0);

        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            LLVMValueRef mask_i = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef not_prev = LLVMBuildNot(ctx->builder, prev_any, "not_prev");
            LLVMValueRef elected = LLVMBuildAnd(ctx->builder, mask_i, not_prev, "elected");

            is_first_vec = LLVMBuildInsertElement(ctx->builder, is_first_vec, elected, LLVMConstInt(ctx->int_type, i, 0), "elect_ins");
            prev_any = LLVMBuildOr(ctx->builder, prev_any, mask_i, "prev_any");
        }
        set_val(ctx, res_id, is_first_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformAll || opcode == SpvOpSubgroupAllKHR)
    {
        LLVMValueRef pred_vec = get_val(ctx, operands[1]);
        LLVMValueRef all_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, pred_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef not_m = LLVMBuildNot(ctx->builder, m, "not_m");
            LLVMValueRef cond = LLVMBuildOr(ctx->builder, not_m, p, "lane_all_cond");
            all_val = LLVMBuildAnd(ctx->builder, all_val, cond, "all_acc");
        }
        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, all_val, LLVMConstInt(ctx->int_type, i, 0), "all_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformAny || opcode == SpvOpSubgroupAnyKHR)
    {
        LLVMValueRef pred_vec = get_val(ctx, operands[1]);
        LLVMValueRef any_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, pred_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef cond = LLVMBuildAnd(ctx->builder, m, p, "lane_any_cond");
            any_val = LLVMBuildOr(ctx->builder, any_val, cond, "any_acc");
        }
        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, any_val, LLVMConstInt(ctx->int_type, i, 0), "any_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformAllEqual || opcode == SpvOpSubgroupAllEqualKHR)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef first_val = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, 0, 0), "first_v");
        LLVMValueRef all_eq = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0);

        for (int i = 1; i < SIMT_WIDTH; i++)
        {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef v = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, i, 0), "v_i");
            LLVMValueRef eq;
            if (is_float_type(LLVMTypeOf(v))) 
            {
                eq = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, first_val, v, "cmp_eq");
            } 
            else 
            {
                eq = LLVMBuildICmp(ctx->builder, LLVMIntEQ, first_val, v, "cmp_eq");
            }
            LLVMValueRef not_m = LLVMBuildNot(ctx->builder, m, "not_m");
            LLVMValueRef cond = LLVMBuildOr(ctx->builder, not_m, eq, "lane_eq_cond");
            all_eq = LLVMBuildAnd(ctx->builder, all_eq, cond, "all_eq_acc");
        }
        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, all_eq, LLVMConstInt(ctx->int_type, i, 0), "eq_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformBroadcast || opcode == SpvOpSubgroupReadInvocationKHR)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef id_val = get_val(ctx, operands[2]);
        LLVMValueRef id_i32 = LLVMBuildExtractElement(ctx->builder, jit_to_int_vector(ctx, id_val), LLVMConstInt(ctx->int_type, 0, 0), "bcast_id");

        LLVMValueRef broadcast_val = LLVMBuildExtractElement(ctx->builder, val_vec, id_i32, "bcast_v");
        LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, broadcast_val, LLVMConstInt(ctx->int_type, i, 0), "bcast_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformBroadcastFirst || opcode == SpvOpSubgroupFirstInvocationKHR)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef first_val = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, 0, 0), "first_v");
        LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));
        for (int i = 0; i < SIMT_WIDTH; i++)
         {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, first_val, LLVMConstInt(ctx->int_type, i, 0), "bcast_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformBallot || opcode == SpvOpSubgroupBallotKHR)
    {
        LLVMValueRef pred_vec = get_val(ctx, operands[1]);
        LLVMValueRef ballot_mask = LLVMConstInt(ctx->int_type, 0, 0);

        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, pred_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef active_bit = LLVMBuildAnd(ctx->builder, m, p, "act_bit");
            LLVMValueRef bit_i32 = LLVMBuildZExt(ctx->builder, active_bit, ctx->int_type, "bit_i32");
            LLVMValueRef shifted = LLVMBuildShl(ctx->builder, bit_i32, LLVMConstInt(ctx->int_type, i, 0), "shifted");
            ballot_mask = LLVMBuildOr(ctx->builder, ballot_mask, shifted, "ballot_acc");
        }

        LLVMValueRef bcast_x = LLVMGetUndef(ctx->vec_float_type);
        LLVMValueRef zero_v = LLVMConstNull(ctx->vec_float_type);
        LLVMValueRef ballot_float = LLVMBuildBitCast(ctx->builder, ballot_mask, ctx->float_type, "b_f_cast");

        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            bcast_x = LLVMBuildInsertElement(ctx->builder, bcast_x, ballot_float, LLVMConstInt(ctx->int_type, i, 0), "bcast_b");
        }

        LLVMValueRef uvec4_res = LLVMGetUndef(LLVMArrayType(ctx->vec_float_type, 4));
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, bcast_x, 0, "uvec4_0");
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, zero_v, 1, "uvec4_1");
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, zero_v, 2, "uvec4_2");
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, zero_v, 3, "uvec4_3");

        set_val(ctx, res_id, uvec4_res);
        return;
    }

    if (opcode == SpvOpGroupNonUniformInverseBallot)
    {
        LLVMValueRef ballot_uvec4 = get_val(ctx, operands[1]);
        LLVMValueRef ballot_x = LLVMBuildExtractValue(ctx->builder, ballot_uvec4, 0, "ballot_x");
        LLVMValueRef ballot_i32 = LLVMBuildBitCast(ctx->builder, ballot_x, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "ballot_i32");

        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            LLVMValueRef bx = LLVMBuildExtractElement(ctx->builder, ballot_i32, LLVMConstInt(ctx->int_type, i, 0), "bx_i");
            LLVMValueRef bit_mask = LLVMConstInt(ctx->int_type, 1 << i, 0);
            LLVMValueRef is_set = LLVMBuildAnd(ctx->builder, bx, bit_mask, "is_set");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntNE, is_set, LLVMConstInt(ctx->int_type, 0, 0), "cmp_set");
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, cmp, LLVMConstInt(ctx->int_type, i, 0), "inv_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformShuffle || opcode == SpvOpGroupNonUniformShuffleXor ||
        opcode == SpvOpGroupNonUniformShuffleUp || opcode == SpvOpGroupNonUniformShuffleDown)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef param_vec = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[2]));
        LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));

        for (int i = 0; i < SIMT_WIDTH; i++) {
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, param_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef src_idx;
            if (opcode == SpvOpGroupNonUniformShuffle) 
            {
                src_idx = LLVMBuildAnd(ctx->builder, p, LLVMConstInt(ctx->int_type, 15, 0), "shuf_idx");
            } 
            else if (opcode == SpvOpGroupNonUniformShuffleXor) 
            {
                src_idx = LLVMBuildXor(ctx->builder, LLVMConstInt(ctx->int_type, i, 0), p, "shuf_xor");
                src_idx = LLVMBuildAnd(ctx->builder, src_idx, LLVMConstInt(ctx->int_type, 15, 0), "shuf_idx");
            } 
            else if (opcode == SpvOpGroupNonUniformShuffleUp) 
            {
                src_idx = LLVMBuildSub(ctx->builder, LLVMConstInt(ctx->int_type, i, 0), p, "shuf_up");
            } 
            else 
            {
                src_idx = LLVMBuildAdd(ctx->builder, LLVMConstInt(ctx->int_type, i, 0), p, "shuf_down");
            }
            LLVMValueRef val_elem = LLVMBuildExtractElement(ctx->builder, val_vec, src_idx, "shuf_elem");
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, val_elem, LLVMConstInt(ctx->int_type, i, 0), "shuf_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    uint32_t group_op = 0;
    LLVMValueRef val_vec = NULL;
    if (operand_count >= 3) 
    {
        group_op = operands[1];
        val_vec = get_val(ctx, operands[2]);
    } 
    else 
    {
        val_vec = get_val(ctx, operands[1]);
    }

    bool is_float = is_float_type(LLVMTypeOf(val_vec));
    LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));

    LLVMValueRef identity_val = NULL;
    switch (opcode) 
    {
        case SpvOpGroupNonUniformIAdd:
        case SpvOpGroupNonUniformFAdd:
        case SpvOpGroupNonUniformBitwiseOr:
        case SpvOpGroupNonUniformBitwiseXor:
        case SpvOpGroupNonUniformLogicalOr:
        case SpvOpGroupNonUniformLogicalXor:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 0.0) : LLVMConstInt(ctx->int_type, 0, 0);
            break;
        case SpvOpGroupNonUniformIMul:
        case SpvOpGroupNonUniformFMul:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 1.0) : LLVMConstInt(ctx->int_type, 1, 0);
            break;
        case SpvOpGroupNonUniformBitwiseAnd:
        case SpvOpGroupNonUniformLogicalAnd:
            identity_val = LLVMConstInt(ctx->int_type, ~0u, 0);
            break;
        case SpvOpGroupNonUniformFMin:
        case SpvOpGroupNonUniformSMin:
        case SpvOpGroupNonUniformUMin:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 1e38) : LLVMConstInt(ctx->int_type, 0x7FFFFFFF, 0);
            break;
        case SpvOpGroupNonUniformFMax:
        case SpvOpGroupNonUniformSMax:
        case SpvOpGroupNonUniformUMax:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, -1e38) : LLVMConstInt(ctx->int_type, 0x80000000, 0);
            break;
        default:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 0.0) : LLVMConstInt(ctx->int_type, 0, 0);
            break;
    }

    LLVMValueRef accum = identity_val;
    LLVMValueRef scan_results[SIMT_WIDTH];

    for (int i = 0; i < SIMT_WIDTH; i++)
    {
        LLVMValueRef elem = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, i, 0), "red_elem");
        LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");

        LLVMValueRef new_accum = NULL;
        switch (opcode) 
        {
            case SpvOpGroupNonUniformFAdd:
                new_accum = LLVMBuildFAdd(ctx->builder, accum, elem, "fadd_acc");
                break;
            case SpvOpGroupNonUniformIAdd:
                new_accum = LLVMBuildAdd(ctx->builder, accum, elem, "iadd_acc");
                break;
            case SpvOpGroupNonUniformFMul:
                new_accum = LLVMBuildFMul(ctx->builder, accum, elem, "fmul_acc");
                break;
            case SpvOpGroupNonUniformIMul:
                new_accum = LLVMBuildMul(ctx->builder, accum, elem, "imul_acc");
                break;
            case SpvOpGroupNonUniformFMin: 
            {
                unsigned min_id = LLVMLookupIntrinsicID("llvm.minnum", 11);
                LLVMValueRef min_func = LLVMGetIntrinsicDeclaration(ctx->module, min_id, &ctx->float_type, 1);
                LLVMValueRef args[2] = { accum, elem };
                new_accum = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(min_func), min_func, args, 2, "fmin_acc");
                break;
            }
            case SpvOpGroupNonUniformFMax: 
            {
                unsigned max_id = LLVMLookupIntrinsicID("llvm.maxnum", 11);
                LLVMValueRef max_func = LLVMGetIntrinsicDeclaration(ctx->module, max_id, &ctx->float_type, 1);
                LLVMValueRef args[2] = { accum, elem };
                new_accum = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(max_func), max_func, args, 2, "fmax_acc");
                break;
            }
            case SpvOpGroupNonUniformSMin: 
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, elem, accum, "smin_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "smin_acc");
                break;
            }
            case SpvOpGroupNonUniformSMax:
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, elem, accum, "smax_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "smax_acc");
                break;
            }
            case SpvOpGroupNonUniformUMin: 
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntULT, elem, accum, "umin_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "umin_acc");
                break;
            }
            case SpvOpGroupNonUniformUMax: 
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntUGT, elem, accum, "umax_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "umax_acc");
                break;
            }
            case SpvOpGroupNonUniformBitwiseAnd:
                new_accum = LLVMBuildAnd(ctx->builder, accum, elem, "band_acc");
                break;
            case SpvOpGroupNonUniformBitwiseOr:
                new_accum = LLVMBuildOr(ctx->builder, accum, elem, "bor_acc");
                break;
            case SpvOpGroupNonUniformBitwiseXor:
                new_accum = LLVMBuildXor(ctx->builder, accum, elem, "bxor_acc");
                break;
            default:
                new_accum = is_float ? LLVMBuildFAdd(ctx->builder, accum, elem, "def_acc") : LLVMBuildAdd(ctx->builder, accum, elem, "def_acc");
                break;
        }

        if (group_op == 2) 
        {
            scan_results[i] = accum;
        }

        accum = LLVMBuildSelect(ctx->builder, m, new_accum, accum, "sel_acc");

        if (group_op == 1) 
        {
            scan_results[i] = accum;
        }
    }

    if (group_op == 0) 
    {
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, accum, LLVMConstInt(ctx->int_type, i, 0), "red_ins");
        }
    } 
    else 
    {
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, scan_results[i], LLVMConstInt(ctx->int_type, i, 0), "scan_ins");
        }
    }

    set_val(ctx, res_id, res_vec);
}