#ifndef JIT_DECORATORS_H
#define JIT_DECORATORS_H

#include "jit.h"

void handle_op_decorate(JitContext* ctx, uint32_t* operands, int op_count);
void handle_op_type_pointer(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_array(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_struct(JitContext* ctx, uint32_t res_id, uint32_t* member_types, int count);
void handle_op_type_void(JitContext* ctx, uint32_t res_id);
void handle_op_type_bool(JitContext* ctx, uint32_t res_id);
void handle_op_type_int(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_float(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_vector(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_member_decorate(JitContext* ctx, uint32_t* operands);

#endif