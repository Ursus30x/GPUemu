#ifndef JIT_DECORATORS_H
#define JIT_DECORATORS_H

#include "jit.h"

void handle_op_decorate(JitContext* ctx, uint32_t* operands, int op_count);
void handle_op_type_pointer(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_array(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_runtime_array(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_struct(JitContext* ctx, uint32_t res_id, uint32_t* member_types, int count);
void handle_op_type_void(JitContext* ctx, uint32_t res_id);
void handle_op_type_bool(JitContext* ctx, uint32_t res_id);
void handle_op_type_int(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_float(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_type_vector(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_member_decorate(JitContext* ctx, uint32_t* operands, int op_count);
void handle_op_name(JitContext* ctx, uint32_t* operands);
void handle_op_type_matrix(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_entry_point(JitContext* ctx, uint32_t* operands, uint32_t operand_count);
uint32_t get_spv_natural_byte_size(JitContext* ctx, uint32_t type_id);
uint32_t get_spv_struct_member_offset(JitContext* ctx, uint32_t struct_type_id, uint32_t member_idx);
uint32_t get_spv_scalar_type_op(JitContext* ctx, uint32_t type_id);
#endif