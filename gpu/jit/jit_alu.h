#ifndef JIT_ARITH_OPS_H
#define JIT_ARITH_OPS_H

#include "jit.h"

void handle_op_constant(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands);
void handle_op_fadd(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fsub(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fmul(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fneg(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_iadd(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_isub(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_imul(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_sdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_udiv(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_sitof(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_select(JitContext* ctx, uint32_t res_id, uint32_t* operands);

#endif 
