#include "jit_alu.h"
#include "debug_gpu.h"

void handle_op_constant(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) 
{
    uint8_t kind = ctx->type_kind_map[type_id];
    LLVMValueRef const_val;
    
    if (kind == SpvOpTypeFloat) {
        float val = *(float*)&operands[0];
        const_val = LLVMConstReal(ctx->float_type, val);
    } else {
        int32_t val = *(int32_t*)&operands[0];
        const_val = LLVMConstInt(ctx->int_type, (unsigned long long)val, 1);
    }

    // Creating the SIMT vector
    LLVMValueRef* vals = malloc(sizeof(LLVMValueRef) * SIMT_WIDTH);
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        vals[i] = const_val;
    }

    LLVMValueRef vec_val = LLVMConstVector(vals, SIMT_WIDTH);
    free(vals);
    
    set_val(ctx, res_id, vec_val);
}

void handle_op_fadd(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFAdd(ctx->builder, lhs, rhs, "v_fadd");
    set_val(ctx, res_id, res);
}

void handle_op_fmul(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFMul(ctx->builder, lhs, rhs, "v_fmul");
    set_val(ctx, res_id, res);
}

void handle_op_fdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFDiv(ctx->builder, lhs, rhs, "v_fdiv");
    set_val(ctx, res_id, res);
}
void handle_op_fsub(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFSub(ctx->builder, lhs, rhs, "v_fsub");
    set_val(ctx, res_id, res);
}
void handle_op_fneg(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildFNeg(ctx->builder, op, "v_fneg");
    set_val(ctx, res_id, res);
}
void handle_op_isub(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildSub(ctx->builder, lhs, rhs, "v_isub");
    set_val(ctx, res_id, res);
}
void handle_op_imul(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildMul(ctx->builder, lhs, rhs, "v_imul");
    set_val(ctx, res_id, res);
}
void handle_op_sdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildSDiv(ctx->builder, lhs, rhs, "v_sdiv");
    set_val(ctx, res_id, res);
}
void handle_op_udiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildUDiv(ctx->builder, lhs, rhs, "v_udiv");
    set_val(ctx, res_id, res);
}
void handle_op_iadd(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildAdd(ctx->builder, lhs, rhs, "v_iadd");
    set_val(ctx, res_id, res);
}

void handle_op_sitof(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef op = get_val(ctx, operands[0]); 
    LLVMValueRef res = LLVMBuildSIToFP(ctx->builder,op, ctx->vec_float_type, "v_sitof");
    set_val(ctx, res_id, res);
}
void handle_op_select(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef cond = get_val(ctx, operands[0]); // Vector of i1
    LLVMValueRef true_val = get_val(ctx, operands[1]);
    LLVMValueRef false_val = get_val(ctx, operands[2]);
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, true_val, false_val, "v_select");
    set_val(ctx, res_id, res);
}