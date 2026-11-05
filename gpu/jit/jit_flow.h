#ifndef JIT_FLOW_H
#define JIT_FLOW_H

#include "jit.h"

void handle_op_type_function(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_function(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands);
void handle_op_label(JitContext* ctx, uint32_t res_id);
void handle_op_branch(JitContext* ctx, uint32_t* operands);
void handle_op_branch_conditional(JitContext* ctx, uint32_t* operands);
void handle_op_selection_merge(JitContext* ctx, uint32_t* operands);
void handle_op_loop_merge(JitContext* ctx, uint32_t* operands);
void handle_op_phi(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);

void resolve_pending_globals(JitContext* ctx);
LLVMValueRef jit_get_emask(JitContext* ctx);
#endif
