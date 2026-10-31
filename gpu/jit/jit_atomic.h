#ifndef JIT_ATOMIC_H
#define JIT_ATOMIC_H

#include "jit.h"
LLVMValueRef jit_to_float_vector(JitContext* ctx, LLVMValueRef val);
LLVMValueRef jit_to_numeric_int_vector(JitContext* ctx, LLVMValueRef val);
LLVMValueRef jit_to_int_vector(JitContext* ctx, LLVMValueRef val);

void handle_op_atomic(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);
void handle_op_group_non_uniform(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);

#endif
