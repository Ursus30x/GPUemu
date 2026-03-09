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
void handle_op_fordlessthan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fordgreaterthan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_fmod(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_op_composite_construct(JitContext* ctx, uint32_t res_id, uint32_t type_id,  uint32_t* operands);
void handle_op_composite_extract(JitContext* ctx, uint32_t res_id, uint32_t* operands, uint32_t num_indices);

void handle_op_vector_times_scalar(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_dot(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_op_matrix_times_vector(JitContext* ctx, uint32_t res_id, uint32_t* operands);

LLVMValueRef calculate_dot_product(JitContext* ctx,  LLVMValueRef vecA , LLVMValueRef vecB);

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
#endif 
