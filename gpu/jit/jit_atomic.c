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

static LLVMValueRef group_lane_index(JitContext* ctx, int lane)
{
    return LLVMConstInt(ctx->int_type, (uint64_t)lane, 0);
}

static LLVMValueRef group_i1_constant(JitContext* ctx, int value)
{
    return LLVMConstInt(LLVMInt1TypeInContext(ctx->context), value, 0);
}

static LLVMValueRef create_group_broadcast_vector(
    JitContext* ctx,
    LLVMTypeRef vector_type,
    LLVMValueRef scalar,
    const char* name)
{
    LLVMValueRef result = LLVMGetUndef(vector_type);

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        result = LLVMBuildInsertElement(
            ctx->builder, result, scalar, group_lane_index(ctx, i), name);
    }

    return result;
}

static LLVMValueRef get_group_ballot_i32(
    JitContext* ctx,
    LLVMValueRef ballot_uvec4,
    const char* name)
{
    LLVMValueRef ballot_x =
        LLVMBuildExtractValue(ctx->builder, ballot_uvec4, 0, "ballot_x");

    LLVMTypeRef vec_i32_type =
        LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    return LLVMBuildBitCast(
        ctx->builder, ballot_x, vec_i32_type, name);
}

static bool handle_group_non_uniform_elect(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id)
{
    if (opcode != SpvOpGroupNonUniformElect)
        return false;

    LLVMValueRef is_first_vec = LLVMGetUndef(ctx->vec_i1_type);
    LLVMValueRef prev_any = group_i1_constant(ctx, 0);

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef lane_idx = group_lane_index(ctx, i);
        LLVMValueRef mask_i = LLVMBuildExtractElement(
            ctx->builder, ctx->emask, lane_idx, "m_i");
        LLVMValueRef not_prev =
            LLVMBuildNot(ctx->builder, prev_any, "not_prev");
        LLVMValueRef elected =
            LLVMBuildAnd(ctx->builder, mask_i, not_prev, "elected");

        is_first_vec = LLVMBuildInsertElement(
            ctx->builder, is_first_vec, elected, lane_idx, "elect_ins");
        prev_any = LLVMBuildOr(ctx->builder, prev_any, mask_i, "prev_any");
    }

    set_val(ctx, res_id, is_first_vec);
    return true;
}


static bool handle_group_non_uniform_all(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformAll &&
        opcode != SpvOpSubgroupAllKHR)
        return false;

    LLVMValueRef pred_vec = get_val(ctx, operands[1]);
    LLVMValueRef all_val = group_i1_constant(ctx, 1);

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef lane_idx = group_lane_index(ctx, i);
        LLVMValueRef mask = LLVMBuildExtractElement(
            ctx->builder, ctx->emask, lane_idx, "m_i");
        LLVMValueRef pred = LLVMBuildExtractElement(
            ctx->builder, pred_vec, lane_idx, "p_i");
        LLVMValueRef not_mask =
            LLVMBuildNot(ctx->builder, mask, "not_m");
        LLVMValueRef condition =
            LLVMBuildOr(ctx->builder, not_mask, pred, "lane_all_cond");

        all_val = LLVMBuildAnd(
            ctx->builder, all_val, condition, "all_acc");
    }

    LLVMValueRef result = create_group_broadcast_vector(
        ctx, ctx->vec_i1_type, all_val, "all_ins");

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_any(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformAny &&
        opcode != SpvOpSubgroupAnyKHR)
        return false;

    LLVMValueRef pred_vec = get_val(ctx, operands[1]);
    LLVMValueRef any_val = group_i1_constant(ctx, 0);

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef lane_idx = group_lane_index(ctx, i);
        LLVMValueRef mask = LLVMBuildExtractElement(
            ctx->builder, ctx->emask, lane_idx, "m_i");
        LLVMValueRef pred = LLVMBuildExtractElement(
            ctx->builder, pred_vec, lane_idx, "p_i");
        LLVMValueRef condition =
            LLVMBuildAnd(ctx->builder, mask, pred, "lane_any_cond");

        any_val = LLVMBuildOr(
            ctx->builder, any_val, condition, "any_acc");
    }

    LLVMValueRef result = create_group_broadcast_vector(
        ctx, ctx->vec_i1_type, any_val, "any_ins");

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_all_equal(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformAllEqual &&
        opcode != SpvOpSubgroupAllEqualKHR)
        return false;

    LLVMValueRef val_vec = get_val(ctx, operands[1]);
    LLVMValueRef first_val = LLVMBuildExtractElement(
        ctx->builder, val_vec, group_lane_index(ctx, 0), "first_v");
    LLVMValueRef all_eq = group_i1_constant(ctx, 1);

    for (int i = 1; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef lane_idx = group_lane_index(ctx, i);
        LLVMValueRef mask = LLVMBuildExtractElement(
            ctx->builder, ctx->emask, lane_idx, "m_i");
        LLVMValueRef value = LLVMBuildExtractElement(
            ctx->builder, val_vec, lane_idx, "v_i");

        LLVMValueRef equal;
        if (is_float_type(LLVMTypeOf(value)))
        {
            equal = LLVMBuildFCmp(
                ctx->builder, LLVMRealOEQ, first_val, value, "cmp_eq");
        }
        else
        {
            equal = LLVMBuildICmp(
                ctx->builder, LLVMIntEQ, first_val, value, "cmp_eq");
        }

        LLVMValueRef not_mask =
            LLVMBuildNot(ctx->builder, mask, "not_m");
        LLVMValueRef condition =
            LLVMBuildOr(ctx->builder, not_mask, equal, "lane_eq_cond");

        all_eq = LLVMBuildAnd(
            ctx->builder, all_eq, condition, "all_eq_acc");
    }

    LLVMValueRef result = create_group_broadcast_vector(
        ctx, ctx->vec_i1_type, all_eq, "eq_ins");

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_broadcast(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformBroadcast &&
        opcode != SpvOpSubgroupReadInvocationKHR)
        return false;

    LLVMValueRef val_vec = get_val(ctx, operands[1]);
    LLVMValueRef id_val = get_val(ctx, operands[2]);
    LLVMValueRef id_i32 = LLVMBuildExtractElement(
        ctx->builder,
        jit_to_int_vector(ctx, id_val),
        group_lane_index(ctx, 0),
        "bcast_id");

    LLVMValueRef broadcast_val = LLVMBuildExtractElement(
        ctx->builder, val_vec, id_i32, "bcast_v");

    LLVMValueRef result = create_group_broadcast_vector(
        ctx, LLVMTypeOf(val_vec), broadcast_val, "bcast_ins");

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_broadcast_first(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    (void)operands;

    if (opcode != SpvOpGroupNonUniformBroadcastFirst &&
        opcode != SpvOpSubgroupFirstInvocationKHR)
        return false;

    LLVMValueRef val_vec = get_val(ctx, operands[1]);
    LLVMValueRef first_val = LLVMBuildExtractElement(
        ctx->builder, val_vec, group_lane_index(ctx, 0), "first_v");

    LLVMValueRef result = create_group_broadcast_vector(
        ctx, LLVMTypeOf(val_vec), first_val, "bcast_ins");

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_ballot(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformBallot &&
        opcode != SpvOpSubgroupBallotKHR)
        return false;

    LLVMValueRef pred_vec = get_val(ctx, operands[1]);
    LLVMValueRef ballot_mask =
        LLVMConstInt(ctx->int_type, 0, 0);

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef lane_idx = group_lane_index(ctx, i);
        LLVMValueRef mask = LLVMBuildExtractElement(
            ctx->builder, ctx->emask, lane_idx, "m_i");
        LLVMValueRef pred = LLVMBuildExtractElement(
            ctx->builder, pred_vec, lane_idx, "p_i");
        LLVMValueRef active_bit =
            LLVMBuildAnd(ctx->builder, mask, pred, "act_bit");
        LLVMValueRef bit_i32 = LLVMBuildZExt(
            ctx->builder, active_bit, ctx->int_type, "bit_i32");
        LLVMValueRef shifted = LLVMBuildShl(
            ctx->builder, bit_i32, group_lane_index(ctx, i), "shifted");

        ballot_mask = LLVMBuildOr(
            ctx->builder, ballot_mask, shifted, "ballot_acc");
    }

    LLVMValueRef ballot_float = LLVMBuildBitCast(
        ctx->builder, ballot_mask, ctx->float_type, "b_f_cast");

    LLVMValueRef broadcast_x = create_group_broadcast_vector(
        ctx, ctx->vec_float_type, ballot_float, "bcast_b");

    LLVMValueRef zero_vec = LLVMConstNull(ctx->vec_float_type);
    LLVMValueRef result =
        LLVMGetUndef(LLVMArrayType(ctx->vec_float_type, 4));

    result = LLVMBuildInsertValue(
        ctx->builder, result, broadcast_x, 0, "uvec4_0");
    result = LLVMBuildInsertValue(
        ctx->builder, result, zero_vec, 1, "uvec4_1");
    result = LLVMBuildInsertValue(
        ctx->builder, result, zero_vec, 2, "uvec4_2");
    result = LLVMBuildInsertValue(
        ctx->builder, result, zero_vec, 3, "uvec4_3");

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_inverse_ballot(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformInverseBallot)
        return false;

    LLVMValueRef ballot_uvec4 = get_val(ctx, operands[1]);
    LLVMValueRef ballot_i32 =
        get_group_ballot_i32(ctx, ballot_uvec4, "ballot_i32");

    LLVMValueRef result = LLVMGetUndef(ctx->vec_i1_type);

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef lane_idx = group_lane_index(ctx, i);
        LLVMValueRef ballot_lane = LLVMBuildExtractElement(
            ctx->builder, ballot_i32, lane_idx, "bx_i");
        LLVMValueRef bit_mask = LLVMConstInt(
            ctx->int_type, (uint64_t)(1u << i), 0);
        LLVMValueRef is_set = LLVMBuildAnd(
            ctx->builder, ballot_lane, bit_mask, "is_set");
        LLVMValueRef is_nonzero = LLVMBuildICmp(
            ctx->builder,
            LLVMIntNE,
            is_set,
            LLVMConstInt(ctx->int_type, 0, 0),
            "cmp_set");

        result = LLVMBuildInsertElement(
            ctx->builder, result, is_nonzero, lane_idx, "inv_ins");
    }

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_ballot_bit_count(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformBallotBitCount)
        return false;

    uint32_t group_op = operands[1];
    LLVMValueRef ballot_uvec4 = get_val(ctx, operands[2]);
    LLVMValueRef ballot_i32 =
        get_group_ballot_i32(ctx, ballot_uvec4, "ballot_i32");

    LLVMValueRef mask_vals[SIMT_WIDTH];
    LLVMValueRef c1_vals[SIMT_WIDTH];
    LLVMValueRef c2_vals[SIMT_WIDTH];
    LLVMValueRef c4_vals[SIMT_WIDTH];
    LLVMValueRef c8_vals[SIMT_WIDTH];
    LLVMValueRef m55_vals[SIMT_WIDTH];
    LLVMValueRef m33_vals[SIMT_WIDTH];
    LLVMValueRef m0f_vals[SIMT_WIDTH];
    LLVMValueRef mff_vals[SIMT_WIDTH];

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        uint32_t lane_mask = 0xFFFFu;

        if (group_op == SpvGroupOperationInclusiveScan)
            lane_mask = (1u << (i + 1)) - 1u;
        else if (group_op == SpvGroupOperationExclusiveScan)
            lane_mask = (1u << i) - 1u;

        mask_vals[i] = LLVMConstInt(ctx->int_type, lane_mask, 0);
        c1_vals[i] = LLVMConstInt(ctx->int_type, 1, 0);
        c2_vals[i] = LLVMConstInt(ctx->int_type, 2, 0);
        c4_vals[i] = LLVMConstInt(ctx->int_type, 4, 0);
        c8_vals[i] = LLVMConstInt(ctx->int_type, 8, 0);
        m55_vals[i] = LLVMConstInt(ctx->int_type, 0x5555, 0);
        m33_vals[i] = LLVMConstInt(ctx->int_type, 0x3333, 0);
        m0f_vals[i] = LLVMConstInt(ctx->int_type, 0x0F0F, 0);
        mff_vals[i] = LLVMConstInt(ctx->int_type, 0x00FF, 0);
    }

    LLVMValueRef value = LLVMBuildAnd(
        ctx->builder, ballot_i32,
        LLVMConstVector(mask_vals, SIMT_WIDTH), "bc_masked");

    LLVMValueRef shr1 = LLVMBuildLShr(
        ctx->builder, value,
        LLVMConstVector(c1_vals, SIMT_WIDTH), "bc_shr1");
    LLVMValueRef sub1 = LLVMBuildSub(
        ctx->builder,
        value,
        LLVMBuildAnd(ctx->builder, shr1,
                     LLVMConstVector(m55_vals, SIMT_WIDTH), "bc_a55"),
        "bc_s1");

    LLVMValueRef shr2 = LLVMBuildLShr(
        ctx->builder, sub1,
        LLVMConstVector(c2_vals, SIMT_WIDTH), "bc_shr2");
    value = LLVMBuildAdd(
        ctx->builder,
        LLVMBuildAnd(ctx->builder, sub1,
                     LLVMConstVector(m33_vals, SIMT_WIDTH), "bc_a33_1"),
        LLVMBuildAnd(ctx->builder, shr2,
                     LLVMConstVector(m33_vals, SIMT_WIDTH), "bc_a33_2"),
        "bc_s2");

    LLVMValueRef shr4 = LLVMBuildLShr(
        ctx->builder, value,
        LLVMConstVector(c4_vals, SIMT_WIDTH), "bc_shr4");
    value = LLVMBuildAnd(
        ctx->builder,
        LLVMBuildAdd(ctx->builder, value, shr4, "bc_s4"),
        LLVMConstVector(m0f_vals, SIMT_WIDTH), "bc_a0f");

    LLVMValueRef shr8 = LLVMBuildLShr(
        ctx->builder, value,
        LLVMConstVector(c8_vals, SIMT_WIDTH), "bc_shr8");
    value = LLVMBuildAnd(
        ctx->builder,
        LLVMBuildAdd(ctx->builder, value, shr8, "bc_s8"),
        LLVMConstVector(mff_vals, SIMT_WIDTH), "bc_res_i32");

    LLVMValueRef result = LLVMBuildUIToFP(
        ctx->builder, value, ctx->vec_float_type, "bc_res_f");

    set_val(ctx, res_id, result);
    return true;
}

static bool handle_group_non_uniform_ballot_bit_extract(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (opcode != SpvOpGroupNonUniformBallotBitExtract)
        return false;

    LLVMValueRef ballot_uvec4 = get_val(ctx, operands[1]);
    LLVMValueRef idx_vec =
        jit_to_numeric_int_vector(ctx, get_val(ctx, operands[2]));
    LLVMValueRef ballot_i32 =
        get_group_ballot_i32(ctx, ballot_uvec4, "ballot_i32");

    LLVMValueRef shifted = LLVMBuildLShr(
        ctx->builder, ballot_i32, idx_vec, "be_shr");

    LLVMValueRef ones[SIMT_WIDTH];
    LLVMValueRef zeros[SIMT_WIDTH];

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        ones[i] = LLVMConstInt(ctx->int_type, 1, 0);
        zeros[i] = LLVMConstInt(ctx->int_type, 0, 0);
    }

    LLVMValueRef bit = LLVMBuildAnd(
        ctx->builder, shifted,
        LLVMConstVector(ones, SIMT_WIDTH), "be_bit");
    LLVMValueRef result = LLVMBuildICmp(
        ctx->builder, LLVMIntNE, bit,
        LLVMConstVector(zeros, SIMT_WIDTH), "be_res");

    set_val(ctx, res_id, result);
    return true;
}

static bool is_group_non_uniform_shuffle_opcode(uint16_t opcode)
{
    return opcode == SpvOpGroupNonUniformShuffle ||
           opcode == SpvOpGroupNonUniformShuffleXor ||
           opcode == SpvOpGroupNonUniformShuffleUp ||
           opcode == SpvOpGroupNonUniformShuffleDown;
}

static LLVMValueRef get_group_shuffle_source_index(
    JitContext* ctx,
    uint16_t opcode,
    int lane,
    LLVMValueRef parameter)
{
    LLVMValueRef lane_idx = group_lane_index(ctx, lane);

    if (opcode == SpvOpGroupNonUniformShuffle)
    {
        return LLVMBuildAnd(
            ctx->builder, parameter,
            LLVMConstInt(ctx->int_type, 15, 0), "shuf_idx");
    }

    if (opcode == SpvOpGroupNonUniformShuffleXor)
    {
        LLVMValueRef source = LLVMBuildXor(
            ctx->builder, lane_idx, parameter, "shuf_xor");

        return LLVMBuildAnd(
            ctx->builder, source,
            LLVMConstInt(ctx->int_type, 15, 0), "shuf_idx");
    }

    if (opcode == SpvOpGroupNonUniformShuffleUp)
    {
        return LLVMBuildSub(
            ctx->builder, lane_idx, parameter, "shuf_up");
    }

    return LLVMBuildAdd(
        ctx->builder, lane_idx, parameter, "shuf_down");
}

static bool handle_group_non_uniform_shuffle(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands)
{
    if (!is_group_non_uniform_shuffle_opcode(opcode))
        return false;

    LLVMValueRef val_vec = get_val(ctx, operands[1]);
    LLVMValueRef param_vec =
        jit_to_numeric_int_vector(ctx, get_val(ctx, operands[2]));
    LLVMValueRef result = LLVMGetUndef(LLVMTypeOf(val_vec));

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef parameter = LLVMBuildExtractElement(
            ctx->builder, param_vec, group_lane_index(ctx, i), "p_i");
        LLVMValueRef source_idx = get_group_shuffle_source_index(
            ctx, opcode, i, parameter);
        LLVMValueRef value = LLVMBuildExtractElement(
            ctx->builder, val_vec, source_idx, "shuf_elem");

        result = LLVMBuildInsertElement(
            ctx->builder, result, value, group_lane_index(ctx, i), "shuf_ins");
    }

    set_val(ctx, res_id, result);
    return true;
}


static LLVMValueRef get_group_non_uniform_identity(
    JitContext* ctx,
    uint16_t opcode,
    bool is_float)
{
    switch (opcode)
    {
        case SpvOpGroupNonUniformIAdd:
        case SpvOpGroupNonUniformFAdd:
        case SpvOpGroupNonUniformBitwiseOr:
        case SpvOpGroupNonUniformBitwiseXor:
        case SpvOpGroupNonUniformLogicalOr:
        case SpvOpGroupNonUniformLogicalXor:
            return is_float
                ? LLVMConstReal(ctx->float_type, 0.0)
                : LLVMConstInt(ctx->int_type, 0, 0);

        case SpvOpGroupNonUniformIMul:
        case SpvOpGroupNonUniformFMul:
            return is_float
                ? LLVMConstReal(ctx->float_type, 1.0)
                : LLVMConstInt(ctx->int_type, 1, 0);

        case SpvOpGroupNonUniformBitwiseAnd:
        case SpvOpGroupNonUniformLogicalAnd:
            return LLVMConstInt(ctx->int_type, ~0u, 0);

        case SpvOpGroupNonUniformFMin:
        case SpvOpGroupNonUniformSMin:
        case SpvOpGroupNonUniformUMin:
            return is_float
                ? LLVMConstReal(ctx->float_type, 1e38)
                : LLVMConstInt(ctx->int_type, 0x7FFFFFFF, 0);

        case SpvOpGroupNonUniformFMax:
        case SpvOpGroupNonUniformSMax:
        case SpvOpGroupNonUniformUMax:
            return is_float
                ? LLVMConstReal(ctx->float_type, -1e38)
                : LLVMConstInt(ctx->int_type, 0x80000000, 0);

        default:
            return is_float
                ? LLVMConstReal(ctx->float_type, 0.0)
                : LLVMConstInt(ctx->int_type, 0, 0);
    }
}

static LLVMValueRef build_group_non_uniform_operation(
    JitContext* ctx,
    uint16_t opcode,
    LLVMValueRef accum,
    LLVMValueRef elem,
    bool is_float)
{
    switch (opcode)
    {
        case SpvOpGroupNonUniformFAdd:
            return LLVMBuildFAdd(ctx->builder, accum, elem, "fadd_acc");

        case SpvOpGroupNonUniformIAdd:
            return LLVMBuildAdd(ctx->builder, accum, elem, "iadd_acc");

        case SpvOpGroupNonUniformFMul:
            return LLVMBuildFMul(ctx->builder, accum, elem, "fmul_acc");

        case SpvOpGroupNonUniformIMul:
            return LLVMBuildMul(ctx->builder, accum, elem, "imul_acc");

        case SpvOpGroupNonUniformFMin:
        {
            unsigned intrinsic_id =
                LLVMLookupIntrinsicID("llvm.minnum", 11);
            LLVMValueRef function = LLVMGetIntrinsicDeclaration(
                ctx->module, intrinsic_id, &ctx->float_type, 1);
            LLVMValueRef args[2] = { accum, elem };

            return LLVMBuildCall2(
                ctx->builder, LLVMGlobalGetValueType(function),
                function, args, 2, "fmin_acc");
        }

        case SpvOpGroupNonUniformFMax:
        {
            unsigned intrinsic_id =
                LLVMLookupIntrinsicID("llvm.maxnum", 11);
            LLVMValueRef function = LLVMGetIntrinsicDeclaration(
                ctx->module, intrinsic_id, &ctx->float_type, 1);
            LLVMValueRef args[2] = { accum, elem };

            return LLVMBuildCall2(
                ctx->builder, LLVMGlobalGetValueType(function),
                function, args, 2, "fmax_acc");
        }

        case SpvOpGroupNonUniformSMin:
        {
            LLVMValueRef cmp = LLVMBuildICmp(
                ctx->builder, LLVMIntSLT, elem, accum, "smin_cmp");
            return LLVMBuildSelect(
                ctx->builder, cmp, elem, accum, "smin_acc");
        }

        case SpvOpGroupNonUniformSMax:
        {
            LLVMValueRef cmp = LLVMBuildICmp(
                ctx->builder, LLVMIntSGT, elem, accum, "smax_cmp");
            return LLVMBuildSelect(
                ctx->builder, cmp, elem, accum, "smax_acc");
        }

        case SpvOpGroupNonUniformUMin:
        {
            LLVMValueRef cmp = LLVMBuildICmp(
                ctx->builder, LLVMIntULT, elem, accum, "umin_cmp");
            return LLVMBuildSelect(
                ctx->builder, cmp, elem, accum, "umin_acc");
        }

        case SpvOpGroupNonUniformUMax:
        {
            LLVMValueRef cmp = LLVMBuildICmp(
                ctx->builder, LLVMIntUGT, elem, accum, "umax_cmp");
            return LLVMBuildSelect(
                ctx->builder, cmp, elem, accum, "umax_acc");
        }

        case SpvOpGroupNonUniformBitwiseAnd:
            return LLVMBuildAnd(ctx->builder, accum, elem, "band_acc");

        case SpvOpGroupNonUniformBitwiseOr:
            return LLVMBuildOr(ctx->builder, accum, elem, "bor_acc");

        case SpvOpGroupNonUniformBitwiseXor:
            return LLVMBuildXor(ctx->builder, accum, elem, "bxor_acc");

        default:
            return is_float
                ? LLVMBuildFAdd(ctx->builder, accum, elem, "def_acc")
                : LLVMBuildAdd(ctx->builder, accum, elem, "def_acc");
    }
}


static LLVMValueRef store_group_non_uniform_scan_results(
    JitContext* ctx,
    LLVMValueRef result_vec,
    LLVMValueRef* scan_results)
{
    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        result_vec = LLVMBuildInsertElement(
            ctx->builder, result_vec, scan_results[i],
            group_lane_index(ctx, i), "scan_ins");
    }

    return result_vec;
}

static LLVMValueRef store_group_non_uniform_reduction_result(
    JitContext* ctx,
    LLVMValueRef result_vec,
    LLVMValueRef accum)
{
    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        result_vec = LLVMBuildInsertElement(
            ctx->builder, result_vec, accum,
            group_lane_index(ctx, i), "red_ins");
    }

    return result_vec;
}

static void handle_group_non_uniform_arithmetic(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t* operands,
    int operand_count)
{
    uint32_t group_op = 0;
    LLVMValueRef val_vec;

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
    LLVMValueRef result = LLVMGetUndef(LLVMTypeOf(val_vec));
    LLVMValueRef identity =
        get_group_non_uniform_identity(ctx, opcode, is_float);

    LLVMValueRef accum = identity;
    LLVMValueRef scan_results[SIMT_WIDTH];

    for (int i = 0; i < SIMT_WIDTH; ++i)
    {
        LLVMValueRef lane_idx = group_lane_index(ctx, i);
        LLVMValueRef elem = LLVMBuildExtractElement(
            ctx->builder, val_vec, lane_idx, "red_elem");
        LLVMValueRef mask = LLVMBuildExtractElement(
            ctx->builder, ctx->emask, lane_idx, "m_i");

        LLVMValueRef new_accum = build_group_non_uniform_operation(
            ctx, opcode, accum, elem, is_float);

        if (group_op == 2)
            scan_results[i] = accum;

        accum = LLVMBuildSelect(
            ctx->builder, mask, new_accum, accum, "sel_acc");

        if (group_op == 1)
            scan_results[i] = accum;
    }

    if (group_op == 0)
    {
        result = store_group_non_uniform_reduction_result(ctx, result, accum);
    }
    else
    {
        result = store_group_non_uniform_scan_results(ctx, result, scan_results);
    }

    set_val(ctx, res_id, result);
}


void handle_op_group_non_uniform(
    JitContext* ctx,
    uint16_t opcode,
    uint32_t res_id,
    uint32_t type_id,
    uint32_t* operands,
    int operand_count)
{
    (void)type_id;

    if (handle_group_non_uniform_elect(ctx, opcode, res_id))
        return;

    if (handle_group_non_uniform_all(ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_any(ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_all_equal(ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_broadcast(ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_broadcast_first(
            ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_ballot(ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_inverse_ballot(
            ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_ballot_bit_count(
            ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_ballot_bit_extract(
            ctx, opcode, res_id, operands))
        return;

    if (handle_group_non_uniform_shuffle(ctx, opcode, res_id, operands))
        return;

    handle_group_non_uniform_arithmetic(
        ctx, opcode, res_id, operands, operand_count);
}
