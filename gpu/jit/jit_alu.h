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
void handle_op_slessthan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_ulessthan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_iequal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fordlessthan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fordgreaterthan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fmod(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_bitwise_and(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_bitwise_or(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_bitwise_xor(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_not(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_shift_left_logical(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_shift_right_logical(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_shift_right_arithmetic(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_bitcast(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands);
void handle_op_convert_f_to_s(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_convert_f_to_u(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_convert_u_to_f(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_inot_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_sgreater_than(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_ugreater_than(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_sgreater_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_ugreater_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_sless_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_uless_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_snegate(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_umod(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_srem(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_smod(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_logical_and(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_logical_or(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_logical_not(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_logical_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_logical_not_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_any(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_all(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_is_nan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_is_inf(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_op_atomic(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);
void handle_op_group_non_uniform(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);


void handle_op_composite_construct(JitContext* ctx, uint32_t res_id, uint32_t type_id,  uint32_t* operands);
void handle_op_composite_extract(JitContext* ctx, uint32_t res_id, uint32_t* operands, uint32_t num_indices);
void handle_op_vector_shuffle(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands);

void handle_op_vector_times_scalar(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_dot(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_matrix_times_vector(JitContext* ctx, uint32_t res_id, uint32_t* operands);


LLVMValueRef calculate_dot_product(JitContext* ctx,  LLVMValueRef vecA , LLVMValueRef vecB);
typedef LLVMValueRef (*LLVMMatFunc_t)(
    LLVMBuilderRef,
    LLVMValueRef /* LHS */,
    LLVMValueRef /* RHS */,
    const char * /* Name */
);

LLVMValueRef vec_mat_helper(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, LLVMMatFunc_t func, const char* name); 
LLVMValueRef mat_operation_helper(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, LLVMMatFunc_t func, const char* name); 

void create_glsl_std_450_map(JitContext* ctx);

void handle_op_ext_instr(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_sin(JitContext* ctx, uint32_t res_id, uint32_t* operands);     
void handle_ext_cos(JitContext* ctx, uint32_t res_id, uint32_t* operands);      
void handle_ext_sqrt(JitContext* ctx, uint32_t res_id, uint32_t* operands);     
void handle_ext_pow(JitContext* ctx, uint32_t res_id, uint32_t* operands);     
void handle_ext_atan2(JitContext* ctx, uint32_t res_id, uint32_t* operands);    
void handle_ext_log(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fabs(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fmax(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fmin(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_smoothstep(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fmix(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fsign(JitContext* ctx, uint32_t res_id, uint32_t* operands); 
void handle_ext_step(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_length(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_normalize(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_reflect(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_distance(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_cross(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_refract(JitContext* ctx, uint32_t res_id, uint32_t* operands);   
void handle_ext_tan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_exp(JitContext* ctx, uint32_t res_id, uint32_t* operands);

#endif 
