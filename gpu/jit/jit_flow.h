#ifndef JIT_FLOW_H
#define JIT_FLOW_H

#include "jit.h"

void handle_op_type_function(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_function(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands);
void handle_op_label(JitContext* ctx, uint32_t res_id);

#endif 
