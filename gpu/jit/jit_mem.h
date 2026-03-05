#ifndef JIT_MEM_H
#define JIT_MEM_H

#include "jit.h"

void handle_op_variable(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t opCount, uint32_t* operands);
void handle_op_load(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands);
void handle_op_store(JitContext* ctx, uint32_t* operands);
void handle_op_access_chain(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);
#endif 
